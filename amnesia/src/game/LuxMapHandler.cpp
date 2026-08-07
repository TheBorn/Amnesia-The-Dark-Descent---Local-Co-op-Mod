/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "LuxMapHandler.h"

#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxPlayerHands.h"
#include "LuxJournal.h"
#include "LuxEffectRenderer.h"
#include "LuxEffectHandler.h"
#include "LuxDebugHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxSavedGame.h"
#include "LuxSaveHandler.h"
#include "LuxConfigHandler.h"
#include "LuxLoadScreenHandler.h"
#include "LuxMainMenu.h"
#include "LuxInventory.h"

#include "LuxEnemy.h"
#include "LuxAchievementHandler.h"
#include "LuxPlayerAvatar.h"
#include "LuxPlayerHelpers.h"
#include "LuxMoveState_Normal.h"
#include "LuxPostEffects.h"
#include "impl/ImGuiDebugMenu.h"

#include "SDL2/SDL.h"

//////////////////////////////////////////////////////////////////////////
// SOUND ENTITY CALLBACK
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cMapHandlerSoundCallback::cMapHandlerSoundCallback()
{
	///////////////////////
	//Load document
	tString sFile = "sounds/EnemySounds.dat";
	iXmlDocument* pXmlDoc = gpBase->mpEngine->GetResources()->LoadXmlDocument(sFile);
	if(pXmlDoc ==NULL)
	{
		Error("Couldn't load XML file '%s'!\n",sFile.c_str());
		return;
	}

	//////////////////////
	// Load data
	cXmlNodeListIterator it = pXmlDoc->GetChildIterator();
	while(it.HasNext())
	{
		cXmlElement *pChildElem = it.Next()->ToElement();

		tString sName = pChildElem->GetAttributeString("name");
		mvEnemyHearableSounds.push_back(sName);
	}

	gpBase->mpEngine->GetResources()->DestroyXmlDocument( pXmlDoc );
}

//-----------------------------------------------------------------------

void cMapHandlerSoundCallback::OnStart(cSoundEntity *apSoundEntity)
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL) return;
	
	///////////////////////////
	//Check if the sound is something to worry bout
	tString sTypeName = apSoundEntity->GetData()->GetName();

	bool bUsed=false;
	for(size_t i=0; i< mvEnemyHearableSounds.size(); ++i)
	{
		tString &sName = mvEnemyHearableSounds[i];
		if(sTypeName.size() >= sName.size() && sName == sTypeName.substr(0,sName.size()))
		{
			bUsed = true;
			break;
		}
	}
	if(bUsed == false) return;
	
	///////////////////////////
	//Iterate enemies and send sound message to those close enough
	float fMaxDist = apSoundEntity->GetMaxDistance();
	float fMinDist = apSoundEntity->GetMaxDistance();
	float fVolume = apSoundEntity->GetVolume();
	cVector3f vPos = apSoundEntity->GetWorldPosition();
	
	pMap->BroadcastEnemySoundMessage(vPos, fVolume, fMinDist, fMaxDist);
}	

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// RENDER CALLBACK
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxDebugRenderCallback::cLuxDebugRenderCallback()
{
}

//-----------------------------------------------------------------------

void cLuxDebugRenderCallback::OnPostSolidDraw(cRendererCallbackFunctions* apFunctions)
{
	if(mpPhysicsWorld)
	{
		apFunctions->SetMatrix(NULL);
		apFunctions->SetBlendMode(eMaterialBlendMode_Alpha);
		apFunctions->SetTextureRange(NULL,0);
		apFunctions->SetProgram(NULL);
		
		apFunctions->SetDepthTest(true);
		apFunctions->SetDepthWrite(false);

		//mpPhysicsWorld->RenderDebugGeometry(apFunctions->GetLowLevelGfx(), cColor(1,1,1,1));
	}

	gpBase->mpDebugHandler->RenderSolid(apFunctions);
	gpBase->mpMapHandler->RenderSolid(apFunctions);

	gpBase->mpPlayer->RenderSolid(apFunctions);

	// Index 0: this callback is on the main viewport and on P1's coop viewport.
	gpBase->mpEffectRenderer->RenderSolid(apFunctions, 0);
}

//-----------------------------------------------------------------------

void cLuxDebugRenderCallback::OnPostTranslucentDraw(cRendererCallbackFunctions* apFunctions)
{
	gpBase->mpPlayer->RenderTrans(apFunctions);
	gpBase->mpEffectRenderer->RenderTrans(apFunctions, 0);
}

//-----------------------------------------------------------------------

void cLuxCoopRenderCallback::OnPostSolidDraw(cRendererCallbackFunctions* apFunctions)
{
	if(gpBase->mpPlayer2)
		gpBase->mpPlayer2->RenderSolid(apFunctions);

	// This viewport never ran the effect renderer at all, which is why P2 saw no
	// pickup glow and no hover outline. Index 1 = P2's outline set.
	gpBase->mpEffectRenderer->RenderSolid(apFunctions, 1);
}

//-----------------------------------------------------------------------

static void SetAvatarVisibleForCamera(cLuxPlayer *apPlayer, cCamera *apViewCam)
{
	if(apPlayer == NULL || apPlayer->GetAvatar() == NULL) return;

	// An avatar is shown in every view except its owner's own (so you never
	// see yourself — not even in background captures, which swap cameras).
	//
	// NOT IsActive(). SetActive(false) is how a script or an effect PAUSES a
	// player -- a vision, a container zoom, a cutscene -- and a paused player is
	// still standing in the room. Keying the avatar off it made P1's body vanish
	// out from under P2 for the length of every scripted sequence.
	//
	// What has to be true is that they exist in this world at all, which is the
	// character body -- the same thing SetEntitiesVisible guards on internally.
	bool bVisible = gpBase->mpMapHandler->GetCoopMode() &&
					apPlayer->GetCharacterBody() != NULL &&
					apPlayer->GetCamera() != apViewCam;

	apPlayer->GetAvatar()->SetEntitiesVisible(bVisible);
}

//-----------------------------------------------------------------------

static void SetGunVisibleForCamera(cLuxPlayer *apPlayer, cCamera *apViewCam)
{
	if(apPlayer == NULL || apPlayer->GetHelperGun() == NULL) return;

	// The gun view model is a floating box in front of a camera -- from anyone
	// else's view that is a gun hanging in mid-air. Owner's view only, exactly
	// like the hands. The muzzle FLASH is a real world light and the tracer is a
	// real world entity, so both stay visible to everyone.
	bool bOwnView = gpBase->mpMapHandler->GetCoopMode()==false ||
					apPlayer->GetCamera() == apViewCam;

	apPlayer->GetHelperGun()->SetViewModelVisibleForCamera(bOwnView);
}

//-----------------------------------------------------------------------

static void SetHandsVisibleForCamera(cLuxPlayer *apPlayer, cCamera *apViewCam)
{
	if(apPlayer == NULL || apPlayer->GetHands() == NULL) return;

	// The hands / lantern model (and its flame bloom) only shows in its
	// owner's own view — the lantern LIGHT keeps shining for everyone.
	// Outside coop there is only the owner's view, so nothing changes.
	bool bOwnView = gpBase->mpMapHandler->GetCoopMode()==false ||
					apPlayer->GetCamera() == apViewCam;

	apPlayer->GetHands()->SetVisualsVisibleForCamera(bOwnView);
}

//-----------------------------------------------------------------------

void cLuxCoopAvatarVisCallback::OnPreWorldDraw()
{
	// Runs right before this viewport renders its world, so the visibility
	// state also applies to the reflections and shadow maps rendered within.
	cCamera *pViewCam = mpViewport ? mpViewport->GetCamera() : NULL;

	SetAvatarVisibleForCamera(gpBase->mpPlayer, pViewCam);
	SetAvatarVisibleForCamera(gpBase->mpPlayer2, pViewCam);

	SetHandsVisibleForCamera(gpBase->mpPlayer, pViewCam);
	SetHandsVisibleForCamera(gpBase->mpPlayer2, pViewCam);

	SetGunVisibleForCamera(gpBase->mpPlayer, pViewCam);
	SetGunVisibleForCamera(gpBase->mpPlayer2, pViewCam);
}

void cLuxCoopRenderCallback::OnPostTranslucentDraw(cRendererCallbackFunctions* apFunctions)
{
	if(gpBase->mpPlayer2)
		gpBase->mpPlayer2->RenderTrans(apFunctions);

	// See OnPostSolidDraw -- this is where the glow and outline actually draw.
	gpBase->mpEffectRenderer->RenderTrans(apFunctions, 1);

}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxMapHandler::cLuxMapHandler() : iLuxUpdateable("LuxMapHandler")
{
	//////////////////////////
	//Create and setup view port
	mpViewport = gpBase->mpEngine->GetScene()->CreateViewport();
	gpBase->mpEngine->GetScene()->SetCurrentListener(mpViewport);

	//////////////////////////
	//Set up post effects
	cGraphics *pGraphics = gpBase->mpEngine->GetGraphics();
	cPostEffectComposite *pPostEffectComp = pGraphics->CreatePostEffectComposite();
	mpViewport->SetPostEffectComposite(pPostEffectComp);
	
	//Bloom
	cPostEffectParams_Bloom bloomParams;
	bloomParams.mfBlurSize = 1.0f;
	bloomParams.mvRgbToIntensity = bloomParams.mvRgbToIntensity * 1.0f;
	mpPostEffect_Bloom = pGraphics->CreatePostEffect(&bloomParams);
	pPostEffectComp->AddPostEffect(mpPostEffect_Bloom, 100);
	
	//Image trail
	cPostEffectParams_ImageTrail imageTrailParams;
	mpPostEffect_ImageTrail = pGraphics->CreatePostEffect(&imageTrailParams);
	pPostEffectComp->AddPostEffect(mpPostEffect_ImageTrail, 10);
	mpPostEffect_ImageTrail->SetActive(false);

	//Radial
	cPostEffectParams_RadialBlur radialBlurParams;
	radialBlurParams.mfSize = 0.0f;
	mpPostEffect_RadialBlur = pGraphics->CreatePostEffect(&radialBlurParams);
	pPostEffectComp->AddPostEffect(mpPostEffect_RadialBlur, 9);
	mpPostEffect_RadialBlur->SetActive(false);

	//Sepia
	cPostEffectParams_ColorConvTex sepiaParams;
	sepiaParams.msTextureFile = "colorconv_sepia.tga";
	sepiaParams.mfFadeAlpha = 0.0f;
	mpPostEffect_Sepia = pGraphics->CreatePostEffect(&sepiaParams);
	pPostEffectComp->AddPostEffect(mpPostEffect_Sepia, 4);
	mpPostEffect_Sepia->SetActive(false);
	
	//////////////////////////
	//Saving
	mpSavedGame = hplNew( cLuxSavedGameMapCollection, () );

	
	//////////////////////////
	//Callbacks
	mpSoundCallback = hplNew( cMapHandlerSoundCallback, () );
	cSoundEntity::AddGlobalCallback(mpSoundCallback);

	//////////////////////////
	//Threading
	mpSavedGameMutex = cPlatform::CreateMutEx();

	//////////////////////////
	//Variables
	mbPausedSoundsAndMusic = false;
	mpDataCache =NULL;

	// Co-op init
	mpCoopPostEffectComp_P1 = NULL;
	mpCoopPostEffectComp_P2 = NULL;
	mpCoopInsanity_P1 = NULL;
	mpCoopInsanity_P2 = NULL;
	mpCoopViewport = NULL;
	mpCoopP1Viewport = NULL;
	mpCoopHudSet = NULL;
	mpCoopDarkOverlayGfx = NULL;
	mbCoopActive = false;
	mbCoopStorySupport = false;
	mlSplitScreenMode = 1;
	mlDualMonitorIndex = -1;
	mbDualMonitorWindowActive = false;
	mbDualMonitorWanted = false;
	mlLastAppliedPostEffectMode = -1;
	mbLastAppliedPostEffectOn = false;
	mlSavedWindowX = 0;
	mlSavedWindowY = 0;
	mlSavedWindowW = 0;
	mlSavedWindowH = 0;
	mlSavedWindowFlags = 0;
	mfOrigFOV = 0.0f;
	mCaptureMode = eBackgroundCaptureMode_None;
	mpCaptureSavedCamera = NULL;
	mpDualMenuMirrorViewport = NULL;

	// Coop Options + player blocker proxies
	mbCoopOptCrouchBoost = false;
	mbCoopOptPlayerCollision = false;
	mbCoopOptMonsterKilling = false;
	mfCoopOptMonsterDamageMul = 10.0f;
	mbCoopOptGlobalLantern = true;

	mpCoopBlockerFull[0] = mpCoopBlockerFull[1] = NULL;
	mpCoopBlockerHead[0] = mpCoopBlockerHead[1] = NULL;
	mpCoopBlockerWorld = NULL;
	mbCoopBoostRiding[0] = mbCoopBoostRiding[1] = false;

	Reset();
}

//-----------------------------------------------------------------------

cLuxMapHandler::~cLuxMapHandler()
{
	if (mpCoopHudSet)
	{
		gpBase->mpEngine->GetGui()->DestroySet(mpCoopHudSet);
		mpCoopHudSet = NULL;
	}
	if (mpCoopViewport)
	{
		gpBase->mpEngine->GetScene()->DestroyViewport(mpCoopViewport);
		mpCoopViewport = NULL;
	}
	if (mpCoopP1Viewport)
	{
		gpBase->mpEngine->GetScene()->DestroyViewport(mpCoopP1Viewport);
		mpCoopP1Viewport = NULL;
	}
	if (mpDualMenuMirrorViewport)
	{
		gpBase->mpEngine->GetScene()->DestroyViewport(mpDualMenuMirrorViewport);
		mpDualMenuMirrorViewport = NULL;
	}
	if (gpBase->mpPlayer2)
	{
		hplDelete(gpBase->mpPlayer2);
		gpBase->mpPlayer2 = NULL;
	}
	hplDelete(mpSavedGame);
	hplDelete(mpSavedGameMutex);
	hplDelete(mpSoundCallback);
	STLDeleteAll(mlstMaps);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMapHandler::OnStart()
{
	//////////////////////
	//Set up viewport 	
	mpViewport->SetCamera(gpBase->mpPlayer->GetCamera());

	mpViewport->AddGuiSet(gpBase->mpGameDebugSet);
	mpViewport->AddGuiSet(gpBase->mpGameHudSet);

    
	mpViewport->AddRendererCallback(&mRenderCallback);

	mAvatarVisCallbackMain.mpViewport = mpViewport;
	mpViewport->AddViewportCallback(&mAvatarVisCallbackMain);

	UpdateViewportRenderProperties();
}

//-----------------------------------------------------------------------

void cLuxMapHandler::UpdateViewportRenderProperties()
{
	// Applies to EVERY live viewport, not just the main one.
	//
	// The coop viewports are created separately and never went through here, so
	// they ran on cRenderSettings defaults -- world reflections and shadows forced
	// ON for both split views no matter what the graphics options said. World
	// reflection re-renders the world for reflective surfaces, so the cost climbs
	// the further into a map you get and the more reflective geometry and
	// shadow-casting lights come into play. That is the co-op-only slowdown that
	// builds up during a map, survives a restart, and vanishes with coop off:
	// not a leak, just two viewports doing expensive work nobody asked for.
	const bool bWorldReflection = gpBase->mpConfigHandler->mbWorldReflection;
	const bool bShadows         = gpBase->mpConfigHandler->mbShadowsActive;

	cViewport *vViewports[3] = { mpViewport, mpCoopP1Viewport, mpCoopViewport };

	for(int i=0; i<3; ++i)
	{
		if(vViewports[i] == NULL) continue;

		cRenderSettings *pRenderSettings = vViewports[i]->GetRenderSettings();
		if(pRenderSettings == NULL) continue;

		pRenderSettings->mbRenderWorldReflection = bWorldReflection;
		pRenderSettings->mbRenderShadows = bShadows;
	}
}

//-----------------------------------------------------------------------

void cLuxMapHandler::SetCoopMode(bool abX)
{
	if (mbCoopActive == abX) return;
	mbCoopActive = abX;

	// The world keeping running under a menu is driven by the menus themselves
	// now (cLuxInventory::Update / cLuxJournal::Update), not by a registered
	// background container. Nothing to set up here.

	// If no map is loaded, just set the flag — coop will activate on next map load
	if (!mpCurrentMap)
	{
		if (!abX)
		{
			// Clean up viewports if they exist
			if (mpCoopHudSet)
			{
				gpBase->mpEngine->GetGui()->DestroySet(mpCoopHudSet);
				mpCoopHudSet = NULL;
			}
			if (mpCoopViewport)
			{
				gpBase->mpEngine->GetScene()->DestroyViewport(mpCoopViewport);
				mpCoopViewport = NULL;
			}
			if (mpCoopP1Viewport)
			{
				gpBase->mpEngine->GetScene()->DestroyViewport(mpCoopP1Viewport);
				mpCoopP1Viewport = NULL;
			}
			if (gpBase->mpPlayer2)
			{
				hplDelete(gpBase->mpPlayer2);
				gpBase->mpPlayer2 = NULL;
			}
		}
		return;
	}

	cVector2f vScreen = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();
	int iScreenW = (int)vScreen.x;
	int iScreenH = (int)vScreen.y;

	cVector2l vP1Pos, vP1Size, vP2Pos, vP2Size;
	GetSplitViewportRects(iScreenW, iScreenH, vP1Pos, vP1Size, vP2Pos, vP2Size);

	if (abX)
	{
		// --- Enable split-screen ---

		mfOrigFOV = gpBase->mpPlayer->GetBaseFOV();

		// Apply dual monitor window extension if mode 3
		if (mlSplitScreenMode == eSplitScreenMode_DualMonitor)
		{
			ApplyDualMonitorWindow(true);
		}

		// Hide the main viewport — we create dedicated viewports for both players
		// The main viewport stays up until BOTH coop viewports exist -- see the
		// handover at the end of this block.

		// Create dedicated P1 viewport — same creation path as P2
		mpCoopP1Viewport = gpBase->mpEngine->GetScene()->CreateViewport(
			gpBase->mpPlayer->GetCamera(),
			mpCurrentMap->GetWorld(),
			false
		);
		mpCoopP1Viewport->SetPosition(vP1Pos);
		mpCoopP1Viewport->SetSize(vP1Size);
		mpCoopP1Viewport->SetRenderer(gpBase->mpEngine->GetGraphics()->GetRenderer(eRenderer_Main));
		//Built hidden; shown in the handover once P2 exists too.
		mpCoopP1Viewport->SetVisible(false);
		mpCoopP1Viewport->SetActive(false);
		mpCoopP1Viewport->AddRendererCallback(&mRenderCallback);
		mAvatarVisCallbackP1.mpViewport = mpCoopP1Viewport;
		mpCoopP1Viewport->AddViewportCallback(&mAvatarVisCallbackP1);

		// Add P1's GUI sets (HUD, debug) to the coop viewport
		mpCoopP1Viewport->AddGuiSet(gpBase->mpGameDebugSet);
		mpCoopP1Viewport->AddGuiSet(gpBase->mpGameHudSet);

		// Post effects — without this the whole post pass is skipped for P1 in
		// split-screen (no bloom, no flashback sepia/radial blur).
		SetupCoopViewportPostEffects(mpCoopP1Viewport, false);

		gpBase->mpEngine->GetScene()->SetCurrentListener(mpCoopP1Viewport);

		// Create P2
		CreatePlayer2(mpCurrentMap);

		cLuxPlayer *pP2 = gpBase->mpPlayer2;
		if (pP2)
		{
			// P2 viewport — identical creation path
			mpCoopViewport = gpBase->mpEngine->GetScene()->CreateViewport(
				pP2->GetCamera(),
				mpCurrentMap->GetWorld(),
				false
			);
			mpCoopViewport->SetPosition(vP2Pos);
			mpCoopViewport->SetSize(vP2Size);
			mpCoopViewport->SetRenderer(gpBase->mpEngine->GetGraphics()->GetRenderer(eRenderer_Main));
			//Built hidden, same as P1.
			mpCoopViewport->SetVisible(false);
			mpCoopViewport->SetActive(false);
			mpCoopViewport->AddRendererCallback(&mCoopRenderCallback);
			mAvatarVisCallbackP2.mpViewport = mpCoopViewport;
			mpCoopViewport->AddViewportCallback(&mAvatarVisCallbackP2);

			// Create a separate HUD GUI set for P2
			mpCoopHudSet = gpBase->mpEngine->GetGui()->CreateSet("CoopHud", NULL);
			mpCoopHudSet->SetVirtualSize(gpBase->mvHudVirtualSize, -1000, 1000, gpBase->mvHudVirtualOffset);
			mpCoopHudSet->SetDrawPriority(0);
			mpCoopViewport->AddGuiSet(mpCoopHudSet);

			// Post effects — see the P1 call above.
			SetupCoopViewportPostEffects(mpCoopViewport, true);

			// Create dark overlay GFX for P2's menu dimming
			if (!mpCoopDarkOverlayGfx)
			{
				mpCoopDarkOverlayGfx = gpBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1,1), eGuiMaterial_Alpha);
			}
		}

		////////////////////////////////////////////////////////////////////////
		// The handover, in one step.
		//
		// This used to hide the main viewport FIRST, then build P1's, then call
		// CreatePlayer2 -- which loads models, entities and sounds and takes real
		// time -- and only then build P2's. Any frame drawn during that saw an
		// incomplete set of viewports:
		//
		//   after hiding the main one : ZERO visible viewports, so the whole screen
		//                               is cleared black and nothing draws over it
		//   after showing P1's        : ONE visible viewport covering HALF the
		//                               screen -- P1's half drawn, P2's half black
		//
		// Black, then P1's half on its own, then both. Which is exactly what
		// enabling co-op looks like from the outside.
		//
		// Nothing is visible until everything is built, then all three flip together,
		// so no frame can observe a half-finished split.
		//BOTH must be genuinely renderable, or nothing changes.
		//
		// cScene::Render only draws a viewport's world when it has a renderer, a
		// world AND a camera. Miss any one and it silently draws nothing, leaving
		// that rect as the black clear -- which is exactly what a screenshot of this
		// bug looks like: P1's half lit and correct, P2's half pure black.
		//
		// So the main viewport is only given up once both halves can actually draw.
		// If P2 is not ready yet, keeping one full-screen view is wrong for co-op but
		// it is a WHOLE picture; trading it for one live half and one black half is
		// strictly worse. The next tick through here fixes it up.
		const bool bP1Ready = mpCoopP1Viewport && mpCoopP1Viewport->GetCamera() &&
							  mpCoopP1Viewport->GetWorld();
		const bool bP2Ready = mpCoopViewport   && mpCoopViewport->GetCamera()   &&
							  mpCoopViewport->GetWorld();

		if(bP1Ready && bP2Ready)
		{
			mpViewport->SetActive(false);
			mpViewport->SetVisible(false);

			mpCoopP1Viewport->SetVisible(true);
			mpCoopP1Viewport->SetActive(true);

			mpCoopViewport->SetVisible(true);
			mpCoopViewport->SetActive(true);
		}
	}
	else
	{
		// --- Disable split-screen ---

		// Restore window if dual monitor was active
		if (mbDualMonitorWindowActive)
		{
			ApplyDualMonitorWindow(false);
		}

		// Destroy P2 HUD set
		if (mpCoopHudSet)
		{
			gpBase->mpEngine->GetGui()->DestroySet(mpCoopHudSet);
			mpCoopHudSet = NULL;
		}

		// Destroy P2 dark overlay
		if (mpCoopDarkOverlayGfx)
		{
			gpBase->mpEngine->GetGui()->DestroyGfx(mpCoopDarkOverlayGfx);
			mpCoopDarkOverlayGfx = NULL;
		}

		// Destroy P2 viewport
		if (mpCoopViewport)
		{
			gpBase->mpEngine->GetScene()->DestroyViewport(mpCoopViewport);
			mpCoopViewport = NULL;
		}

		// Destroy P1 coop viewport
		if (mpCoopP1Viewport)
		{
			gpBase->mpEngine->GetScene()->DestroyViewport(mpCoopP1Viewport);
			mpCoopP1Viewport = NULL;
		}

		// Destroy P2
		if (mpCurrentMap) DestroyPlayer2(mpCurrentMap);

		// Restore main viewport to full screen
		mpViewport->SetPosition(cVector2l(0, 0));
		mpViewport->SetSize(cVector2l(-1, -1));
		mpViewport->SetVisible(true);
		mpViewport->SetActive(true);

		// Restore listener to main viewport
		gpBase->mpEngine->GetScene()->SetCurrentListener(mpViewport);
	}
}

//-----------------------------------------------------------------------

void cLuxMapHandler::GetSplitViewportRects(int aiScreenW, int aiScreenH,
	cVector2l &avP1Pos, cVector2l &avP1Size,
	cVector2l &avP2Pos, cVector2l &avP2Size)
{
	int iSplitW = aiScreenW / 2;
	int iSplitH = aiScreenH / 2;

	switch (mlSplitScreenMode)
	{
	case eSplitScreenMode_TopBottom:
	{
		int iOffsetX = (aiScreenW - iSplitW) / 2;
		avP1Pos  = cVector2l(iOffsetX, 0);
		avP1Size = cVector2l(iSplitW, iSplitH);
		avP2Pos  = cVector2l(iOffsetX, iSplitH);
		avP2Size = cVector2l(iSplitW, iSplitH);
		break;
	}
	case eSplitScreenMode_DualMonitor:
	{
		// P1 gets full main screen
		avP1Pos  = cVector2l(0, 0);
		avP1Size = cVector2l(aiScreenW, aiScreenH);

		// P2 positioned on the second monitor
		// Get the second monitor's bounds relative to the primary
		if (mlDualMonitorIndex >= 0 && mlDualMonitorIndex < SDL_GetNumVideoDisplays())
		{
			SDL_Rect displayBounds;
			SDL_GetDisplayBounds(mlDualMonitorIndex, &displayBounds);

			// Get primary monitor bounds
			SDL_Window* pWin = SDL_GL_GetCurrentWindow();
			int iMainDisplay = pWin ? SDL_GetWindowDisplayIndex(pWin) : 0;
			SDL_Rect mainBounds;
			SDL_GetDisplayBounds(iMainDisplay, &mainBounds);

			// P2 position is relative to the extended window's origin (which is mainBounds origin)
			int iRelX = displayBounds.x - mainBounds.x;
			int iRelY = displayBounds.y - mainBounds.y;

			avP2Pos  = cVector2l(iRelX, iRelY);
			avP2Size = cVector2l(displayBounds.w, displayBounds.h);
		}
		else
		{
			// Fallback: right side split
			int iOffsetY = (aiScreenH - iSplitH) / 2;
			avP2Pos  = cVector2l(iSplitW, iOffsetY);
			avP2Size = cVector2l(iSplitW, iSplitH);
		}
		break;
	}
	case eSplitScreenMode_LeftRight:
	default:
	{
		int iOffsetY = (aiScreenH - iSplitH) / 2;
		avP1Pos  = cVector2l(0, iOffsetY);
		avP1Size = cVector2l(iSplitW, iSplitH);
		avP2Pos  = cVector2l(iSplitW, iOffsetY);
		avP2Size = cVector2l(iSplitW, iSplitH);
		break;
	}
	}
}

//-----------------------------------------------------------------------

void cLuxMapHandler::ApplyDualMonitorWindow(bool abEnable)
{
	//Record only -- see the note on the declaration. UpdateDualMonitorWindow settles
	//it once a frame, so an off/on pair inside one frame costs nothing.
	mbDualMonitorWanted = abEnable;
}

//-----------------------------------------------------------------------

void cLuxMapHandler::UpdateDualMonitorWindow()
{
	if (mbDualMonitorWanted == mbDualMonitorWindowActive) return;

	ApplyDualMonitorWindowNow(mbDualMonitorWanted);
}

//-----------------------------------------------------------------------

void cLuxMapHandler::ApplyDualMonitorWindowNow(bool abEnable)
{
	SDL_Window* pWin = SDL_GL_GetCurrentWindow();
	if (!pWin) return;

	if (abEnable && !mbDualMonitorWindowActive)
	{
		if (mlDualMonitorIndex < 0 || mlDualMonitorIndex >= SDL_GetNumVideoDisplays())
			return;

		// Save current window state
		SDL_GetWindowPosition(pWin, &mlSavedWindowX, &mlSavedWindowY);
		SDL_GetWindowSize(pWin, &mlSavedWindowW, &mlSavedWindowH);
		mlSavedWindowFlags = SDL_GetWindowFlags(pWin);

		// Get bounds of both displays
		int iMainDisplay = SDL_GetWindowDisplayIndex(pWin);
		SDL_Rect mainBounds, secBounds;
		SDL_GetDisplayBounds(iMainDisplay, &mainBounds);
		SDL_GetDisplayBounds(mlDualMonitorIndex, &secBounds);

		// Compute bounding rect spanning both monitors
		int iMinX = (mainBounds.x < secBounds.x) ? mainBounds.x : secBounds.x;
		int iMinY = (mainBounds.y < secBounds.y) ? mainBounds.y : secBounds.y;
		int iMaxX = ((mainBounds.x + mainBounds.w) > (secBounds.x + secBounds.w))
			? (mainBounds.x + mainBounds.w) : (secBounds.x + secBounds.w);
		int iMaxY = ((mainBounds.y + mainBounds.h) > (secBounds.y + secBounds.h))
			? (mainBounds.y + mainBounds.h) : (secBounds.y + secBounds.h);

		int iTotalW = iMaxX - iMinX;
		int iTotalH = iMaxY - iMinY;

		// Every one of these four calls rebuilds the swap chain on its own, so each is
		// skipped when it would not change anything. Setting a window to the size it
		// already has is not a no-op to the driver.
		if (mlSavedWindowFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP))
			SDL_SetWindowFullscreen(pWin, 0);

		if ((mlSavedWindowFlags & SDL_WINDOW_BORDERLESS) == 0)
			SDL_SetWindowBordered(pWin, SDL_FALSE);

		if (mlSavedWindowX != iMinX || mlSavedWindowY != iMinY)
			SDL_SetWindowPosition(pWin, iMinX, iMinY);

		if (mlSavedWindowW != iTotalW || mlSavedWindowH != iTotalH)
			SDL_SetWindowSize(pWin, iTotalW, iTotalH);

		mbDualMonitorWindowActive = true;
	}
	else if (!abEnable && mbDualMonitorWindowActive)
	{
		// Restore original window state
		SDL_SetWindowBordered(pWin, (mlSavedWindowFlags & SDL_WINDOW_BORDERLESS) ? SDL_FALSE : SDL_TRUE);
		SDL_SetWindowSize(pWin, mlSavedWindowW, mlSavedWindowH);
		SDL_SetWindowPosition(pWin, mlSavedWindowX, mlSavedWindowY);

		// Restore fullscreen if it was on
		if (mlSavedWindowFlags & SDL_WINDOW_FULLSCREEN)
			SDL_SetWindowFullscreen(pWin, SDL_WINDOW_FULLSCREEN);
		else if (mlSavedWindowFlags & SDL_WINDOW_FULLSCREEN_DESKTOP)
			SDL_SetWindowFullscreen(pWin, SDL_WINDOW_FULLSCREEN_DESKTOP);

		mbDualMonitorWindowActive = false;
	}
}

//-----------------------------------------------------------------------

void cLuxMapHandler::SetSplitScreenMode(int alMode)
{
	if (alMode < 1 || alMode > 3) return;

	int iOldMode = mlSplitScreenMode;
	mlSplitScreenMode = alMode;

	// If coop is active, reposition viewports and manage window
	if (mbCoopActive && mpCoopP1Viewport && mpCoopViewport)
	{
		// Leaving dual monitor mode — restore window first
		if (iOldMode == eSplitScreenMode_DualMonitor && alMode != eSplitScreenMode_DualMonitor)
		{
			ApplyDualMonitorWindow(false);
		}

		// Entering dual monitor mode — extend window
		if (alMode == eSplitScreenMode_DualMonitor && iOldMode != eSplitScreenMode_DualMonitor)
		{
			ApplyDualMonitorWindow(true);
		}

		cVector2f vScreen = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();
		int iScreenW = (int)vScreen.x;
		int iScreenH = (int)vScreen.y;

		cVector2l vP1Pos, vP1Size, vP2Pos, vP2Size;
		GetSplitViewportRects(iScreenW, iScreenH, vP1Pos, vP1Size, vP2Pos, vP2Size);

		mpCoopP1Viewport->SetPosition(vP1Pos);
		mpCoopP1Viewport->SetSize(vP1Size);
		mpCoopViewport->SetPosition(vP2Pos);
		mpCoopViewport->SetSize(vP2Size);
	}
}

//-----------------------------------------------------------------------

void cLuxMapHandler::SetDualMonitorIndex(int alIdx)
{
	mlDualMonitorIndex = alIdx;
}

//-----------------------------------------------------------------------

cLuxMapHandler::eBackgroundCaptureMode cLuxMapHandler::GetBackgroundCaptureMode()
{
	if (!mbCoopActive || mpCoopP1Viewport == NULL || mpCoopViewport == NULL)
		return eBackgroundCaptureMode_None;

	// An inventory is open/opening: capture the OPENING player's view alone,
	// rendered full-screen. (It is displayed squeezed into that player's
	// viewport rect, which has the same aspect ratio as the full screen, so
	// it looks correct.) Full-screen menus (journal, pause) have no player
	// context and fall through to Composite/DualP1.
	if (gpBase->mpInventory && gpBase->mpInventory->HasPlayerContext())
	{
		return gpBase->mpInventory->GetActivePlayerIndex() == 1 ?
			eBackgroundCaptureMode_P2Full : eBackgroundCaptureMode_P1Full;
	}

	if (mlSplitScreenMode == eSplitScreenMode_DualMonitor)
		return eBackgroundCaptureMode_DualP1;

	// A full-screen menu in split-screen: capture the full composite view.
	return eBackgroundCaptureMode_Composite;
}

//-----------------------------------------------------------------------

void cLuxMapHandler::PrepareBackgroundCapture()
{
	mCaptureMode = GetBackgroundCaptureMode();

	switch (mCaptureMode)
	{
	case eBackgroundCaptureMode_DualP1:
	{
		// Dual monitor: hide P2 so only P1's full-screen view is captured.
		if (mpCoopViewport)
		{
			mpCoopViewport->SetVisible(false);
			mpCoopViewport->SetActive(false);
		}

		// Ensure the main viewport (P1 camera) is the visible render source.
		// It usually already is (OnLeaveContainer shows it for full-screen
		// menus), but assert it here so captures triggered from other states
		// (e.g. opening the journal straight out of an inventory) work too.
		mpViewport->SetPosition(cVector2l(0, 0));
		mpViewport->SetSize(cVector2l(-1, -1));
		mpViewport->SetVisible(true);
		mpViewport->SetActive(true);
		break;
	}
	case eBackgroundCaptureMode_P1Full:
	case eBackgroundCaptureMode_P2Full:
	{
		// An inventory: render the OPENING player's view alone through the
		// main viewport, full-screen. This function is the single owner of
		// that state — previously cLuxInventory::RenderBackgroundImage set
		// this up and PrepareBackgroundCapture immediately undid it by
		// re-establishing the split viewports, so the captured background was
		// the composite view instead of the opening player's own view.
		cLuxPlayer *pOpener = (mCaptureMode == eBackgroundCaptureMode_P2Full) ?
								gpBase->mpPlayer2 : gpBase->mpPlayer;

		if (mpCoopP1Viewport)
		{
			mpCoopP1Viewport->SetVisible(false);
			mpCoopP1Viewport->SetActive(false);
		}
		if (mpCoopViewport)
		{
			mpCoopViewport->SetVisible(false);
			mpCoopViewport->SetActive(false);
		}

		mpCaptureSavedCamera = mpViewport->GetCamera();
		if (pOpener) mpViewport->SetCamera(pOpener->GetCamera());
		if (mpCurrentMap) mpViewport->SetWorld(mpCurrentMap->GetWorld());
		mpViewport->SetPosition(cVector2l(0, 0));
		mpViewport->SetSize(cVector2l(-1, -1));
		mpViewport->SetVisible(true);
		mpViewport->SetActive(true);
		break;
	}
	case eBackgroundCaptureMode_Composite:
	{
		// Split-screen: capture the full composite view (both viewports + black bars).
		// FULLY re-establish BOTH coop viewports here (camera, world, rect,
		// visibility) and hide the main viewport. Previously this only toggled
		// visibility, so if either coop viewport had a stale/cleared camera or
		// world, Scene::Render skipped it and the captured frame collapsed to the
		// single (P1) view — which is exactly the "snapshot of the 1-player view"
		// bug. Rebuilding the state guarantees both players are in the snapshot.
		mpViewport->SetVisible(false);
		mpViewport->SetActive(false);

		cWorld *pWorld = mpCurrentMap ? mpCurrentMap->GetWorld() : NULL;

		cVector2f vScreen = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();
		cVector2l vP1Pos, vP1Size, vP2Pos, vP2Size;
		GetSplitViewportRects((int)vScreen.x, (int)vScreen.y, vP1Pos, vP1Size, vP2Pos, vP2Size);

		if (mpCoopP1Viewport)
		{
			mpCoopP1Viewport->SetCamera(gpBase->mpPlayer->GetCamera());
			if (pWorld) mpCoopP1Viewport->SetWorld(pWorld);
			mpCoopP1Viewport->SetPosition(vP1Pos);
			mpCoopP1Viewport->SetSize(vP1Size);
			mpCoopP1Viewport->SetVisible(true);
			mpCoopP1Viewport->SetActive(true);
		}
		if (mpCoopViewport && gpBase->mpPlayer2)
		{
			mpCoopViewport->SetCamera(gpBase->mpPlayer2->GetCamera());
			if (pWorld) mpCoopViewport->SetWorld(pWorld);
			mpCoopViewport->SetPosition(vP2Pos);
			mpCoopViewport->SetSize(vP2Size);
			mpCoopViewport->SetVisible(true);
			mpCoopViewport->SetActive(true);
		}
		break;
	}
	default:
		break;
	}
}

//-----------------------------------------------------------------------

void cLuxMapHandler::RestoreBackgroundCapture()
{
	switch (mCaptureMode)
	{
	case eBackgroundCaptureMode_DualP1:
	{
		// Restore P2's viewport. The main viewport intentionally STAYS
		// visible: in dual-monitor coop the menus draw only a translucent dim
		// over the world, so P1's monitor needs the live (frozen) world
		// rendering beneath the menu UI — hiding it here would leave P1's
		// side black.
		if (mpCoopViewport)
		{
			mpCoopViewport->SetVisible(true);
			mpCoopViewport->SetActive(true);
		}
		break;
	}
	case eBackgroundCaptureMode_P1Full:
	case eBackgroundCaptureMode_P2Full:
	{
		// Put the original camera back on the main viewport and hide it
		// again; re-show the OTHER player's live gameplay viewport (the
		// invariant while an inventory is open in coop: the non-shopping
		// player's viewport stays visible). The opening player's viewport
		// stays hidden — the inventory UI renders in its place.
		if (mpCaptureSavedCamera) mpViewport->SetCamera(mpCaptureSavedCamera);
		mpCaptureSavedCamera = NULL;
		mpViewport->SetVisible(false);
		mpViewport->SetActive(false);

		cViewport *pOtherVp = (mCaptureMode == eBackgroundCaptureMode_P2Full) ?
								mpCoopP1Viewport : mpCoopViewport;
		if (pOtherVp)
		{
			pOtherVp->SetVisible(true);
			pOtherVp->SetActive(true);
		}
		break;
	}
	case eBackgroundCaptureMode_Composite:
	{
		// Split-screen: hide coop viewports again, restore main viewport for menu UI
		if (mpCoopP1Viewport)
		{
			mpCoopP1Viewport->SetVisible(false);
			mpCoopP1Viewport->SetActive(false);
		}
		if (mpCoopViewport)
		{
			mpCoopViewport->SetVisible(false);
			mpCoopViewport->SetActive(false);
		}

		mpViewport->SetPosition(cVector2l(0, 0));
		mpViewport->SetSize(cVector2l(-1, -1));
		mpViewport->SetActive(true);
		mpViewport->SetVisible(true);
		break;
	}
	default:
		break;
	}

	mCaptureMode = eBackgroundCaptureMode_None;
}

//-----------------------------------------------------------------------

void cLuxMapHandler::AddDualMenuMirror(cGuiSet *apSet)
{
	if (apSet == NULL) return;
	if (!mbCoopActive || mlSplitScreenMode != eSplitScreenMode_DualMonitor) return;
	if (mpCoopViewport == NULL) return;

	////////////////////////////////////////////////////////////////////
	// Lazily create a GUI-only viewport covering P2's monitor. It has no
	// camera/world/renderer, so Scene::Render just binds its rect and draws
	// the GUI sets — and since it is created after the coop viewports it
	// renders on top of P2's (frozen) world view, like a pause overlay.
	// cGui only clears render objects after the buffer swap, so the same GUI
	// set can safely render in both the menu's own viewport and this mirror
	// within one frame.
	if (mpDualMenuMirrorViewport == NULL)
	{
		mpDualMenuMirrorViewport = gpBase->mpEngine->GetScene()->CreateViewport();
		mpDualMenuMirrorViewport->SetRenderer(NULL);
	}

	// Always re-position over P2's monitor (rect may change with settings).
	mpDualMenuMirrorViewport->SetPosition(mpCoopViewport->GetPosition());
	mpDualMenuMirrorViewport->SetSize(mpCoopViewport->GetSize());

	////////////////////////////////////////////////////////////////////
	// The mirror shows exactly ONE set: this one. RemoveDualMenuMirror only
	// detaches the set it is handed, so a loadscreen -> menu -> journal chain
	// could leave earlier sets still attached and silently re-shown the next
	// time the mirror came up -- several menus stacked on P2's monitor at once.
	// cViewport::AddGuiSet also has no duplicate check, so re-adding the same
	// set would render it twice into this viewport. Strip everything first.
	{
		tGuiSetList lstOldSets;
		cGuiSetListIterator setIt = mpDualMenuMirrorViewport->GetGuiSetIterator();
		while(setIt.HasNext()) lstOldSets.push_back(setIt.Next());

		for(tGuiSetListIt oldIt = lstOldSets.begin(); oldIt != lstOldSets.end(); ++oldIt)
			mpDualMenuMirrorViewport->RemoveGuiSet(*oldIt);
	}

	mpDualMenuMirrorViewport->AddGuiSet(apSet);
	mpDualMenuMirrorViewport->SetVisible(true);
	mpDualMenuMirrorViewport->SetActive(true);
}

//-----------------------------------------------------------------------

void cLuxMapHandler::RemoveDualMenuMirror(cGuiSet *apSet)
{
	if (mpDualMenuMirrorViewport == NULL) return;

	if (apSet) mpDualMenuMirrorViewport->RemoveGuiSet(apSet);
	mpDualMenuMirrorViewport->SetVisible(false);
	mpDualMenuMirrorViewport->SetActive(false);
}

//-----------------------------------------------------------------------

void cLuxMapHandler::CreatePlayer2(cLuxMap* apMap)
{
	if (!apMap || gpBase->mpPlayer2) return;

	// Create a full cLuxPlayer for P2
	// Pass player index 1 so IsPlayer2() returns true DURING construction
	// (needed to skip hands creation — entity loaders are global singletons).
	cLuxPlayer *pP2 = hplNew(cLuxPlayer, (1));
	pP2->SetPlayerIndex(1);
	gpBase->mpPlayer2 = pP2;

	// Initialize P2 (mirrors what the module system does)
	pP2->LoadFonts();
	pP2->Reset();
	pP2->CreateWorldEntities(apMap);
	pP2->OnMapEnter(apMap);

	// OnMapEnter already calls OnEnterState on the move state, but double-check
	// that the character body's max speeds are set by forcing a re-enter.
	pP2->ReenterCurrentMoveState();

	// Place P2 next to P1
	PlacePlayer2NearPlayer1();
}

//-----------------------------------------------------------------------

void cLuxMapHandler::PlacePlayer2NearPlayer1()
{
	cLuxPlayer *pP2 = gpBase->mpPlayer2;
	if (!pP2 || !pP2->GetCharacterBody()) return;

	iCharacterBody *pP1Body = gpBase->mpPlayer->GetCharacterBody();
	if (!pP1Body) return;

	// Offset P2 to P1's right so they don't spawn inside each other.
	cVector3f vP1Pos = pP1Body->GetFeetPosition();
	float fYaw = pP1Body->GetYaw();
	cVector3f vRight(cosf(fYaw), 0, -sinf(fYaw));
	pP2->GetCharacterBody()->SetFeetPosition(vP1Pos + vRight * 1.0f);
	pP2->GetCharacterBody()->SetYaw(fYaw);
	pP2->GetCamera()->SetYaw(fYaw);
	pP2->GetCamera()->SetPitch(0);

	pP2->GetCharacterBody()->Update(0.001f);
}

//-----------------------------------------------------------------------

void cLuxMapHandler::TeleportPlayerToOther(cLuxPlayer *apMover, cLuxPlayer *apTarget)
{
	if(apMover==NULL || apTarget==NULL || apMover==apTarget) return;

	iCharacterBody *pMoverBody = apMover->GetCharacterBody();
	iCharacterBody *pTargetBody = apTarget->GetCharacterBody();
	if(pMoverBody==NULL || pTargetBody==NULL) return;

	// Land a step to the target's right, facing the same way they do, with
	// any falling/knockback speed cleared.
	cVector3f vPos = pTargetBody->GetFeetPosition();
	float fYaw = pTargetBody->GetYaw();
	cVector3f vRight(cosf(fYaw), 0, -sinf(fYaw));

	pMoverBody->SetFeetPosition(vPos + vRight * 0.8f);
	pMoverBody->SetForceVelocity(0);
	pMoverBody->SetYaw(fYaw);
	apMover->GetCamera()->SetYaw(fYaw);
	pMoverBody->Update(0.001f);

	// Liquid areas only clear the in-water flag for a body they can still see,
	// so teleporting out of water left the mover swimming forever: water speed
	// multiplier and water footsteps on dry land. The area re-sets it next
	// update if the target happens to be standing in water too.
	apMover->SetIsInWater(false);

	gpBase->mpDebugHandler->AddMessage(
		apMover->IsPlayer2() ? _W("Teleported P2 to P1") : _W("Teleported P1 to P2"), false);
}

//-----------------------------------------------------------------------

void cLuxMapHandler::SetupCoopViewportPostEffects(cViewport *apViewport, bool abPlayer2)
{
	if(apViewport==NULL) return;

	////////////////////////////
	// Post effects now work in EVERY split mode, dual monitor included.
	//
	// They used to be switched off for dual monitor, and the reason was real at
	// the time: iPostEffect::GetTextureUvPosAndSize located a viewport's slice of
	// the render texture as vViewportPos / GetScreenSizeFloat(). A dual-monitor
	// window is stretched across two displays while GetScreenSizeFloat() keeps
	// reporting one, so for P2 -- whose viewport starts at a whole monitor's width
	// -- that division came out at 1.0 and sampling began at the far edge of the
	// texture. P2 got clamped pixels, black with horizontal banding, while P1 at
	// x=0 divided to 0.0 and looked fine.
	//
	// That maths is gone. The renderer rasterises each viewport's world into the
	// whole buffer at the origin and the viewport rect is applied at blit time, so
	// GetTextureUvPosAndSize now returns the whole source at the origin for every
	// viewport and there is no slice to get wrong. Nothing about the dual-monitor
	// window is special to it any more -- which is why bloom, sepia and radial
	// blur were missing on both halves in that mode and present in the others.
	if(ImGuiDebugMenu::GetCoopPostEffects()==false)
	{
		apViewport->SetPostEffectComposite(NULL);
		return;
	}

	cGraphics *pGraphics = gpBase->mpEngine->GetGraphics();
	cPostEffectComposite *pComp = abPlayer2 ? mpCoopPostEffectComp_P2 : mpCoopPostEffectComp_P1;

	if(pComp==NULL)
	{
		pComp = pGraphics->CreatePostEffectComposite();

		// Shared instances with the main composite, so GetPostEffect_Sepia() /
		// _RadialBlur() drive both halves at once and they stay in lockstep. Bloom
		// is included too -- it is always active, so it also means the post pass
		// runs every frame, which is exactly the single-monitor case now known to
		// render correctly. Image trail stays out: it accumulates across frames and
		// one shared instance would smear the two halves together.
		if(mpPostEffect_Bloom)			pComp->AddPostEffect(mpPostEffect_Bloom, 100);
		if(mpPostEffect_RadialBlur)		pComp->AddPostEffect(mpPostEffect_RadialBlur, 9);
		if(mpPostEffect_Sepia)			pComp->AddPostEffect(mpPostEffect_Sepia, 4);

		////////////////////////////////////////////////////////////////////
		// Per-player insanity distortion. The shared instance sits on the MAIN
		// viewport's composite, which split-screen hides -- so in coop nobody
		// ever saw the sanity wave, and the two sanity helpers overwrote each
		// other's values on it. NOT shared between the halves: the wave/zoom
		// amounts are per player, driven by that player's own cLuxPlayerSanity
		// via GetInsanityEffectForPlayer(). Registered with the post effect
		// handler for Update() ticking only; honours the same Options toggle
		// as the main instance.
		{
			cLuxPostEffect_Insanity **ppInsanity = abPlayer2 ? &mpCoopInsanity_P2 : &mpCoopInsanity_P1;
			if(*ppInsanity == NULL)
			{
				cResources *pResources = gpBase->mpEngine->GetResources();
				*ppInsanity = hplNew(cLuxPostEffect_Insanity, (pGraphics, pResources));
				if(gpBase->mpPostEffectHandler && gpBase->mpPostEffectHandler->GetInsanity())
					(*ppInsanity)->SetDisabled(gpBase->mpPostEffectHandler->GetInsanity()->IsDisabled());
				if(gpBase->mpPostEffectHandler)
					gpBase->mpPostEffectHandler->AddCoopUpdateEffect(*ppInsanity);
			}
			pComp->AddPostEffect(*ppInsanity, 25);
		}

		if(abPlayer2)	mpCoopPostEffectComp_P2 = pComp;
		else			mpCoopPostEffectComp_P1 = pComp;
	}

	apViewport->SetPostEffectComposite(pComp);
}

//-----------------------------------------------------------------------

bool cLuxMapHandler::GetCoopPostEffectsActive()
{
	if(mbCoopActive==false) return false;

	//No split-mode exception any more -- see SetupCoopViewportPostEffects. This
	//has to agree with what that function actually attaches, or the hud sepia
	//fallback either doubles up with the real effect or leaves a gap.
	return ImGuiDebugMenu::GetCoopPostEffects();
}

//-----------------------------------------------------------------------

cLuxPostEffect_Insanity* cLuxMapHandler::GetInsanityEffectForPlayer(cLuxPlayer *apPlayer)
{
	////////////////////////////////////////////////////////////////////////
	// Split coop with per-view post effects: each player drives the instance
	// on THEIR OWN composite. Otherwise (single player; coop with the post
	// pass off, e.g. dual monitor): P1 keeps the main-viewport instance, and
	// a P2 without a composite of their own gets NULL -- driving nothing beats
	// silently fighting P1 over one set of values.
	if(mbCoopActive && GetCoopPostEffectsActive())
	{
		if(apPlayer && apPlayer->IsPlayer2()) return mpCoopInsanity_P2;
		return mpCoopInsanity_P1;
	}

	if(mbCoopActive && apPlayer && apPlayer->IsPlayer2()) return NULL;

	return gpBase->mpPostEffectHandler ? gpBase->mpPostEffectHandler->GetInsanity() : NULL;
}

//-----------------------------------------------------------------------

void cLuxMapHandler::SetCoopInsanityDisabled(bool abX)
{
	if(mpCoopInsanity_P1) mpCoopInsanity_P1->SetDisabled(abX);
	if(mpCoopInsanity_P2) mpCoopInsanity_P2->SetDisabled(abX);
}

//-----------------------------------------------------------------------

bool cLuxMapHandler::CoopPlayerCollisionEffective()
{
	if(mbCoopActive==false) return false;
	//Native story: the story's scripted choice. Forced coop: the debug toggle.
	if(mbCoopStorySupport) return mbCoopOptPlayerCollision;
	return ImGuiDebugMenu::GetEnableCoopPlayerCollisions();
}

bool cLuxMapHandler::CoopCrouchBoostEffective()
{
	if(mbCoopActive==false) return false;
	if(mbCoopStorySupport) return mbCoopOptCrouchBoost;
	return ImGuiDebugMenu::GetCoopCrouchBoost();
}

//-----------------------------------------------------------------------

void cLuxMapHandler::AbandonCoopPlayerBlockers()
{
	//Null-only on purpose: this is called on map leave / reset, when the bodies
	//are about to die (or already died) with their physics world. Destroying
	//them here would double-free; recreating happens lazily in
	//UpdateCoopPlayerBlockers once a map and both players exist again.
	mpCoopBlockerFull[0] = mpCoopBlockerFull[1] = NULL;
	mpCoopBlockerHead[0] = mpCoopBlockerHead[1] = NULL;
	mpCoopBlockerWorld = NULL;
	mbCoopBoostRiding[0] = mbCoopBoostRiding[1] = false;
}

//-----------------------------------------------------------------------

void cLuxMapHandler::UpdateCoopPlayerBlockers(float afTimeStep)
{
	////////////////////////////////////////////////////////////////////////
	// Want blockers at all this tick?
	cLuxPlayer *pPlayers[2] = { gpBase->mpPlayer, gpBase->mpPlayer2 };

	bool bWant = mbCoopActive && mpCurrentMap!=NULL &&
				 pPlayers[0]!=NULL && pPlayers[1]!=NULL &&
				 pPlayers[0]->GetCharacterBody()!=NULL &&
				 pPlayers[1]->GetCharacterBody()!=NULL;

	const bool bFull  = bWant && CoopPlayerCollisionEffective();
	const bool bBoost = bWant && CoopCrouchBoostEffective();

	if(bWant==false || (bFull==false && bBoost==false))
	{
		for(int i=0; i<2; ++i)
		{
			if(mpCoopBlockerFull[i]) mpCoopBlockerFull[i]->SetActive(false);
			if(mpCoopBlockerHead[i]) mpCoopBlockerHead[i]->SetActive(false);
			mbCoopBoostRiding[i] = false;
		}
		return;
	}

	iPhysicsWorld *pWorld = mpCurrentMap->GetPhysicsWorld();

	////////////////////////////////////////////////////////////////////////
	// (Re)create in the current map's world. If the world changed, the old
	// bodies died with it -- the pointers were already abandoned on map leave,
	// but guard on the world anyway so a missed path cannot dangle.
	if(mpCoopBlockerWorld != pWorld)
	{
		AbandonCoopPlayerBlockers();
		mpCoopBlockerWorld = pWorld;
	}

	for(int i=0; i<2; ++i)
	{
		if(mpCoopBlockerFull[i]!=NULL) continue;

		iCharacterBody *pCharBody = pPlayers[i]->GetCharacterBody();
		cVector3f vStandSize = pCharBody->GetShape(0)->GetSize();	//shape 0 = standing

		//Slightly slimmer than the real body so brushing past a wall the other
		//player leans on does not wedge anyone; height matches standing.
		cVector3f vFullSize(vStandSize.x*0.85f, vStandSize.y - 0.05f, vStandSize.z*0.85f);
		//Thin lid over the head. Slightly slimmer still, so genuine side
		//contact slides off instead of catching.
		cVector3f vHeadSize(vStandSize.x*0.8f, 0.06f, vStandSize.z*0.8f);

		tString sSuffix = (i==0) ? "P1" : "P2";

		iPhysicsBody *pFull = pWorld->CreateBody("CoopBlockerFull"+sSuffix, pWorld->CreateBoxShape(vFullSize, NULL));
		iPhysicsBody *pHead = pWorld->CreateBody("CoopBlockerHead"+sSuffix, pWorld->CreateBoxShape(vHeadSize, NULL));

		iPhysicsBody *pBoth[2] = { pFull, pHead };
		for(int b=0; b<2; ++b)
		{
			//Mass 0 = kinematic. Invisible to Newton contacts and every
			//non-character query (SetCollide false), visible to character
			//movement (SetCollideCharacter true). The flag bit is what lets the
			//represented player's own char body ignore its own blockers --
			//cLuxPlayer::CreateCharacterBody masks the matching bit out.
			pBoth[b]->SetMass(0);
			pBoth[b]->SetCollide(false);
			pBoth[b]->SetCollideCharacter(true);
			pBoth[b]->SetCollideFlags(i==0 ? eFlagBit_14 : eFlagBit_15);
			pBoth[b]->SetActive(false);
		}

		mpCoopBlockerFull[i] = pFull;
		mpCoopBlockerHead[i] = pHead;
	}

	////////////////////////////////////////////////////////////////////////
	// Per-player state
	for(int i=0; i<2; ++i)
	{
		cLuxPlayer *pPlr = pPlayers[i];
		cLuxPlayer *pOther = pPlayers[1-i];
		iCharacterBody *pBody = pPlr->GetCharacterBody();
		iCharacterBody *pOtherBody = pOther->GetCharacterBody();

		//A dead player blocks nothing: their body is frozen where they fell and
		//must never wall the survivor in -- or block the respawn placement.
		const bool bPlayerOk = pPlr->IsDead()==false;

		const cVector3f vFeet = pBody->GetFeetPosition();
		const float fHeadTopY = vFeet.y + pBody->GetSize().y;
		const cVector3f vStandSize = pBody->GetShape(0)->GetSize();

		////////////////////////////////////////////////////////////////////
		// FULL body blocker: anchored so its TOP is the current head top --
		// crouching genuinely lowers what the other player can stand on, and
		// the box bottom poking below the floor is harmless (only character
		// movement sees it).
		{
			const bool bAct = bFull && bPlayerOk;
			if(bAct)
			{
				cVector3f vCenter(pBody->GetPosition().x,
								  fHeadTopY - (vStandSize.y - 0.05f)*0.5f,
								  pBody->GetPosition().z);
				mpCoopBlockerFull[i]->SetMatrix(cMath::MatrixTranslate(vCenter));
			}
			mpCoopBlockerFull[i]->SetActive(bAct);
		}

		////////////////////////////////////////////////////////////////////
		// HEAD platform: only without full collision (full already covers it).
		//
		// Engages when this player is CROUCHED and the other player comes down
		// from ABOVE: their feet at or above this head, and horizontally on
		// top. Walking into each other never touches it, and side contact at
		// ground level fails the feet-height gate. Once ridden, it stays until
		// the rider steps/falls off (hysteresis) -- so the crouched player may
		// STAND UP and carry the rider with them, Counter-Strike style; the
		// platform tracks the head top and lifts its passenger.
		{
			bool bAct = false;

			if(bFull==false && bBoost && bPlayerOk && pOther->IsDead()==false)
			{
				const cVector3f vOtherFeet = pOtherBody->GetFeetPosition();

				const float fDX = vOtherFeet.x - pBody->GetPosition().x;
				const float fDZ = vOtherFeet.z - pBody->GetPosition().z;
				const float fRadSum = (vStandSize.x + pOtherBody->GetSize().x) * 0.5f;
				const bool bOnTop = (fDX*fDX + fDZ*fDZ) <= fRadSum*fRadSum;

				if(mbCoopBoostRiding[1-i])
				{
					//Riding: keep the floor under them while they stay on it,
					//crouched or not.
					bAct = bOnTop && vOtherFeet.y >= fHeadTopY - 0.35f;
				}
				else
				{
					bool bCrouched = false;
					if(pPlr->GetCurrentMoveState() == eLuxMoveState_Normal)
					{
						cLuxMoveState_Normal *pNormal =
							static_cast<cLuxMoveState_Normal*>(pPlr->GetCurrentMoveStateData());
						bCrouched = pNormal->IsCrouching();
					}

					bAct = bCrouched && bOnTop && vOtherFeet.y >= fHeadTopY - 0.15f;
				}

				mbCoopBoostRiding[1-i] = bAct;
			}
			else
			{
				mbCoopBoostRiding[1-i] = false;
			}

			if(bAct)
			{
				cVector3f vCenter(pBody->GetPosition().x, fHeadTopY + 0.03f, pBody->GetPosition().z);
				mpCoopBlockerHead[i]->SetMatrix(cMath::MatrixTranslate(vCenter));
			}
			mpCoopBlockerHead[i]->SetActive(bAct);
		}
	}
}

//-----------------------------------------------------------------------

void cLuxMapHandler::DestroyPlayer2(cLuxMap* apMap)
{
	if (!gpBase->mpPlayer2) return;

	// Defensive: null P2's camera on viewport to prevent dangling pointer
	if (mpCoopViewport)
	{
		mpCoopViewport->SetCamera(NULL);
		mpCoopViewport->SetVisible(false);
		mpCoopViewport->SetActive(false);
	}

	if (apMap)
	{
		gpBase->mpPlayer2->OnMapLeave(apMap);
		gpBase->mpPlayer2->DestroyWorldEntities(apMap);
	}

	hplDelete(gpBase->mpPlayer2);
	gpBase->mpPlayer2 = NULL;
}

//-----------------------------------------------------------------------

void cLuxMapHandler::UpdatePlayer2(float afTimeStep)
{
	cLuxPlayer *pP2 = gpBase->mpPlayer2;
	if (!pP2 || !pP2->GetCharacterBody()) return;

	// NO FOV SYNC. This used to force P2's camera FOV to P1's every frame, from
	// before scripts knew which player they were acting on. It caused both of the
	// FOV bugs: P1's cutscene FOV leaked onto a P2 who triggered nothing, and
	// P2's own FOV effect was overwritten a frame later -- so when one player
	// started a sequence and the other ended it, the two mfFOVMul values diverged
	// and the FOV looked stuck.
	//
	// cLuxPlayer owns its FOV completely (mfFOV * mfFOVMul, applied in its own
	// Update), and P2 runs a full Update below, so it manages itself correctly.

	// Update P2 as a full player (states, move states, helpers, etc.)
	pP2->Update(afTimeStep);
	pP2->PostUpdate(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxMapHandler::Update(float afTimeStep)
{
	//Settle the spanning window here, and only here. Everything else records a wish.
	UpdateDualMonitorWindow();

	//TODO: Bad placement! Moooove!
	gpBase->mpEffectRenderer->ClearRenderLists();

	CheckMapChange(afTimeStep);

	if(mpCurrentMap && mMapChangeData.mbActive==false)
	{
		mpCurrentMap->Update(afTimeStep);

		// Update Player 2 if coop is active
		if (mbCoopActive && gpBase->mpPlayer2)
		{
			UpdatePlayer2(afTimeStep);
		}

		// Re-assert post-effect attachment every frame so flipping the toggle or
		// switching split layout takes effect live, with no coop restart.
		if (mbCoopActive)
		{
			//Only when something actually changed.
			//
			// This ran unconditionally, and Update() is a LOGIC tick -- up to six of them
			// per rendered frame. So both viewports had their post-effect composite
			// reassigned six times a frame forever, for a value that changes only when you
			// touch the toggle or switch split layout.
			//
			// That is also why pausing or tabbing out settles the picture: both stop the
			// logic loop while rendering carries on, so the churn stops with it.
			const bool bWantPostEffects = ::ImGuiDebugMenu::GetCoopPostEffects();
			if(	mlLastAppliedPostEffectMode != mlSplitScreenMode ||
				mbLastAppliedPostEffectOn   != bWantPostEffects)
			{
				mlLastAppliedPostEffectMode = mlSplitScreenMode;
				mbLastAppliedPostEffectOn   = bWantPostEffects;

				SetupCoopViewportPostEffects(mpCoopP1Viewport, false);
				SetupCoopViewportPostEffects(mpCoopViewport, true);
			}

			// Cheap (two bool writes per viewport) and guarantees the coop
			// viewports can never be left on default render settings, whichever
			// path created them.
			UpdateViewportRenderProperties();
		}

		// Player-vs-player collision proxies (full collision + crouch boost).
		// Runs whether or not coop is on -- it deactivates everything itself
		// when unwanted, so a coop toggle can never leave a stale blocker up.
		UpdateCoopPlayerBlockers(afTimeStep);
	}
}

//-----------------------------------------------------------------------

void cLuxMapHandler::Reset()
{
	// Restore the window if the dual-monitor extension is still active —
	// otherwise quitting to the main menu leaves the menu stretched across
	// two monitors. (Coop re-applies it on the next map load.)
	if (mbDualMonitorWindowActive)
	{
		ApplyDualMonitorWindow(false);

		//Settled here and now rather than deferred. cLuxMapHandler only updates in the
		//"Default" container, so once this returns and the game switches to MainMenu
		//Update() stops running -- a recorded wish would never be acted on and the menu
		//would sit stretched across both monitors. Nothing re-enables it in the same
		//frame on this path, so there is no pair to collapse and nothing is lost.
		UpdateDualMonitorWindow();
	}

	// Clean up coop resources and clear the coop flag (cleared below).
	if (mpCoopHudSet)
	{
		gpBase->mpEngine->GetGui()->DestroySet(mpCoopHudSet);
		mpCoopHudSet = NULL;
	}
	if (mpCoopViewport)
	{
		gpBase->mpEngine->GetScene()->DestroyViewport(mpCoopViewport);
		mpCoopViewport = NULL;
	}
	if (mpCoopP1Viewport)
	{
		gpBase->mpEngine->GetScene()->DestroyViewport(mpCoopP1Viewport);
		mpCoopP1Viewport = NULL;
	}
	if (mpDualMenuMirrorViewport)
	{
		gpBase->mpEngine->GetScene()->DestroyViewport(mpDualMenuMirrorViewport);
		mpDualMenuMirrorViewport = NULL;
	}
	// The scene's sound listener is moved onto the coop P1 viewport by
	// SetCoopMode(true) and moved back by SetCoopMode(false) -- but Reset()
	// destroys that viewport WITHOUT restoring it, so cScene::mpCurrentListener
	// was left pointing at freed memory. cScene::PostUpdate dereferences it
	// unguarded every frame, so the stale pointer survived quit-to-menu, save
	// loads and even a brand new game -- only a process restart cleared it.
	// (SetCurrentListener itself is dangling-safe: it guards with ViewportExists.)
	gpBase->mpEngine->GetScene()->SetCurrentListener(mpViewport);

	// Background-capture state must not outlive the map either.
	// PrepareBackgroundCapture() swaps the OPENING player's camera onto the main
	// viewport and stashes the old one. If a quit/load happens before
	// RestoreBackgroundCapture() runs, mpCaptureSavedCamera can name a camera
	// that DestroyPlayer2() is about to free, and the next Restore installs it on
	// the main viewport permanently -- mpViewport->SetCamera() exists in only
	// three places and one of them is OnStart(), which runs once per PROCESS.
	// Clear the capture and re-assert P1's camera.
	mCaptureMode = eBackgroundCaptureMode_None;
	mpCaptureSavedCamera = NULL;
	if (gpBase->mpPlayer) mpViewport->SetCamera(gpBase->mpPlayer->GetCamera());

	if (gpBase->mpPlayer2 && mpCurrentMap)
	{
		DestroyPlayer2(mpCurrentMap);
	}
	else if (gpBase->mpPlayer2)
	{
		hplDelete(gpBase->mpPlayer2);
		gpBase->mpPlayer2 = NULL;
	}
	// Coop crash fix: clear the coop flag on quit-to-menu / reset. Reset() frees
	// P2 and the split-screen viewports; previously it KEPT mbCoopActive = true,
	// which left the menu in a half-torn-down coop state and made the next load
	// skip SetCoopMode() -> dangling P2 / viewport -> crash on load-from-menu.
	// Clearing it makes re-entry rebuild coop cleanly from the save via
	// SetCoopMode() (the same path that already works on the first load).
	mbCoopActive = false;

	//Cleared here too: it belongs to the story that declared it, and leaving it set
	//would carry a custom story's co-op semantics into whatever the player starts
	//next -- including the main game.
	mbCoopStorySupport = false;

	//Coop Options back to defaults -- they belong to the story that set them.
	//A save restores its own values after this (LoadSaveGameData), and a fresh
	//story re-declares in OnGameStart.
	mbCoopOptCrouchBoost = false;
	mbCoopOptPlayerCollision = false;
	mbCoopOptMonsterKilling = false;
	mfCoopOptMonsterDamageMul = 10.0f;
	mbCoopOptGlobalLantern = true;

	//The blocker bodies are about to die with their map's physics world.
	AbandonCoopPlayerBlockers();

	// Restore P1 viewport to full (re-split on the next map load via SetCoopMode)
	mpViewport->SetPosition(cVector2l(0, 0));
	mpViewport->SetSize(cVector2l(-1, -1));
	mpViewport->SetVisible(true);
	mpViewport->SetActive(true);

	// Stop all sounds (deleting maps will stop world entries, but will let GUI ones live)
	cSound *pSound = gpBase->mpEngine->GetSound();
	pSound->GetSoundHandler()->StopAll(eSoundEntryType_All);

	// Ensure no viewport holds a world reference before we delete all maps
	mpViewport->SetWorld(NULL);

	STLDeleteAll(mlstMaps);
	mpCurrentMap = NULL;

	ResumeSoundsAndMusic(); //Make sure that all sounds and music are resumed!

	msMapFolder = "";

	mbUpdateActive = true;

	mMapChangeData.mbActive = false;

	mpSavedGame->Reset();

	gpBase->mpHelpFuncs->CleanupData();

	DestroyDataCache();
}

//-----------------------------------------------------------------------

void cLuxMapHandler::OnQuit()
{
    // The blocker bodies die with the map's physics world during cleanup.
    AbandonCoopPlayerBlockers();

    // Immediately deactivate coop viewports to prevent render issues during cleanup
    if (mpCoopViewport)
    {
        mpCoopViewport->SetCamera(NULL);
        mpCoopViewport->SetWorld(NULL);
        mpCoopViewport->SetVisible(false);
        mpCoopViewport->SetActive(false);
    }
    if (mpCoopP1Viewport)
    {
        mpCoopP1Viewport->SetCamera(NULL);
        mpCoopP1Viewport->SetWorld(NULL);
        mpCoopP1Viewport->SetVisible(false);
        mpCoopP1Viewport->SetActive(false);
    }
    gpBase->mpEngine->GetUpdater()->SetContainer("MainMenu");

    // Re-apply guards: OnLeaveContainer may have re-enabled viewports
    if (mpCoopViewport)
    {
        mpCoopViewport->SetCamera(NULL);
        mpCoopViewport->SetWorld(NULL);
        mpCoopViewport->SetVisible(false);
        mpCoopViewport->SetActive(false);
    }
    if (mpCoopP1Viewport)
    {
        mpCoopP1Viewport->SetCamera(NULL);
        mpCoopP1Viewport->SetWorld(NULL);
        mpCoopP1Viewport->SetVisible(false);
        mpCoopP1Viewport->SetActive(false);
    }
    // Null main viewport world too — DrawMenuScreen will set up its own rendering
    mpViewport->SetWorld(NULL);

    gpBase->mpLoadScreenHandler->DrawMenuScreen();
    
    //Destroy map
    cLuxMapHandler *mpMapHandler = gpBase->mpMapHandler;
    if(mpMapHandler->GetCurrentMap())
    {
        //Save
        gpBase->mpSaveHandler->AutoSave();
        
        mpMapHandler->DestroyMap(mpMapHandler->GetCurrentMap(),false);

        //Reset game
        gpBase->mpEngine->GetUpdater()->BroadcastMessageToAll(eUpdateableMessage_Reset);
        gpBase->SetCustomStory(NULL);
    }

    //Start up menu again
    gpBase->mpMainMenu->OnLeaveContainer("");
    gpBase->mpMainMenu->OnEnterContainer("");
}


//-----------------------------------------------------------------------

void cLuxMapHandler::LoadUserConfig()
{
	mbShowCommentary = gpBase->mpUserConfig->GetBool("Game","ShowCommentary", false);
}

void cLuxMapHandler::SaveUserConfig()
{
	gpBase->mpUserConfig->SetBool("Game","ShowCommentary", mbShowCommentary);
}

//-----------------------------------------------------------------------

void cLuxMapHandler::CreateDataCache()
{
	if(mpDataCache) DestroyDataCache();
	
	mpDataCache = hplNew(cLuxModelCache, () );
	mpDataCache->Create();
}

void cLuxMapHandler::DestroyDataCache()
{
	if(mpDataCache==NULL) return;
	hplDelete(mpDataCache);
	mpDataCache = NULL;
}

//-----------------------------------------------------------------------

void cLuxMapHandler::SetUpdateActive(bool abX)
{
	mbUpdateActive = abX;
	
	if(mpCurrentMap) mpCurrentMap->GetWorld()->SetActive(mbUpdateActive);
}

//-----------------------------------------------------------------------

void cLuxMapHandler::RenderSolid(cRendererCallbackFunctions* apFunctions)
{
	//mpViewport->GetRenderSettings()->mbLog = false;
	if(mpCurrentMap) mpCurrentMap->OnRenderSolid(apFunctions);
}

//-----------------------------------------------------------------------

void cLuxMapHandler::OnEnterContainer(const tString& asOldContainer)
{
	// Restore split-screen when returning to gameplay
	if (mbCoopActive && mpCoopP1Viewport && mpCoopViewport)
	{
		// Main viewport stays hidden; show coop viewports
		mpViewport->SetActive(false);
		mpViewport->SetVisible(false);

		mpCoopP1Viewport->SetActive(true);
		mpCoopP1Viewport->SetVisible(true);

		mpCoopViewport->SetActive(true);
		mpCoopViewport->SetVisible(true);
	}
	else
	{
		mpViewport->SetActive(true);
		mpViewport->SetVisible(true);
	}

	if(mpCurrentMap) mpCurrentMap->GetWorld()->SetActive(true);

	ResumeSoundsAndMusic();
}

void cLuxMapHandler::OnLeaveContainer(const tString& asNewContainer)
{
	//////////////////////////////////////////////////////////////
	// The load screen owns the whole frame, for BOTH players.
	//
	// cLuxLoadScreenHandler creates its viewport at startup, so it sits BEFORE
	// the co-op viewports in cScene::mlstViewports -- and cScene::Render walks
	// that list in order, so the split views painted straight over it. P1 got
	// the black screen and the hint text while P2 kept watching the live world.
	//
	// Unlike the menu cases below this is UNCONDITIONAL: dual monitor hides P2
	// as well. P2 does not lose the text -- cLuxLoadScreenHandler mirrors its
	// GUI set onto P2's monitor for exactly this case.
	//
	// OnEnterContainer rebuilds the split on the way back to Default.
	if (asNewContainer == "LoadScreen")
	{
		if (mpCoopP1Viewport)
		{
			mpCoopP1Viewport->SetActive(false);
			mpCoopP1Viewport->SetVisible(false);
		}
		if (mpCoopViewport)
		{
			mpCoopViewport->SetActive(false);
			mpCoopViewport->SetVisible(false);
		}

		// Same as what single player already got from the generic branch below.
		mpViewport->SetActive(false);
		mpViewport->SetVisible(false);

		if (mpCurrentMap) mpCurrentMap->GetWorld()->SetActive(false);
		return;
	}

	if (mbCoopActive && mpCoopP1Viewport && mpCoopViewport)
	{
		// If viewports have been nulled (e.g. during OnQuit), don't touch them
		if (mpCoopP1Viewport->GetCamera() == NULL || mpCoopViewport->GetCamera() == NULL)
		{
			mpViewport->SetActive(false);
			mpViewport->SetVisible(false);
			return;
		}

		// A journal on one player's half behaves exactly like the bag. A note or
		// diary picked up in the WORLD does not -- that goes to both players, full
		// screen, and falls through to the branch below.
		bool bPerPlayerJournal = (asNewContainer == "Journal" &&
								 gpBase->mpJournal->GetShowOnBothPlayers()==false);

		if (asNewContainer == "Inventory" || bPerPlayerJournal)
		{
			// Per-viewport menu: hide the OPENING player's viewport —
			// the menu renders in its place — and keep the other
			// player's viewport live. The main viewport stays hidden.
			bool bOwnerIsP2 = bPerPlayerJournal ?
							gpBase->mpJournal->GetActivePlayer()->IsPlayer2() :
							(gpBase->mpInventory->GetActivePlayerIndex() == 1);

			if (bOwnerIsP2)
			{
				mpCoopViewport->SetActive(false);
				mpCoopViewport->SetVisible(false);
			}
			else
			{
				mpCoopP1Viewport->SetActive(false);
				mpCoopP1Viewport->SetVisible(false);
			}
		}
		else
		{
			// Full-screen container (Journal, MainMenu, Credits, ...)
			mpCoopP1Viewport->SetActive(false);
			mpCoopP1Viewport->SetVisible(false);

			if (mlSplitScreenMode == eSplitScreenMode_DualMonitor)
			{
				// Dual monitor: P2 keeps their own screen visible
			}
			else
			{
				// Split-screen: hide P2 too — the menu takes over the full screen
				mpCoopViewport->SetActive(false);
				mpCoopViewport->SetVisible(false);
			}

			// Show main viewport for the menu UI rendering
			mpViewport->SetPosition(cVector2l(0, 0));
			mpViewport->SetSize(cVector2l(-1, -1));
			mpViewport->SetActive(true);
			mpViewport->SetVisible(true);
		}
	}
	else
	{
		mpViewport->SetActive(false);
		mpViewport->SetVisible(false);
	}

	// Keep the world active while a per-player menu is open, so the OTHER
	// player's half keeps simulating and rendering a live scene.
	//
	// cScene::PostUpdate runs cWorld::Update -- physics, entities, particles,
	// lights -- only `if(pWorld->IsActive())`. Switching the world off here stops
	// ALL of that for both players: P2's character body never integrates the
	// movement their stick asked for, and nothing on their screen animates. That
	// is the frozen half, and it had nothing to do with input, containers or
	// update order -- the world itself was switched off.
	//
	// This listed "Inventory" alone, from before the journal was per-player.
	bool bKeepWorldActive = false;
	if (mbCoopActive && (asNewContainer == "Inventory" || asNewContainer == "Journal"))
		bKeepWorldActive = true;
	if(mpCurrentMap && !bKeepWorldActive)
		mpCurrentMap->GetWorld()->SetActive(false);
}


//-----------------------------------------------------------------------

void cLuxMapHandler::ChangeMap(const tString& asMapName, const tString& asStartPos, const tString& asStartSound, const tString& asEndSound)
{
	mMapChangeData.mbActive = true;
	mMapChangeData.msMapFile = cString::SetFileExt(asMapName, "map");
	mMapChangeData.msStartPos = asStartPos;
    mMapChangeData.msSound = asEndSound;

    gpBase->mpHelpFuncs->PlayGuiSoundData(asStartSound, eSoundEntryType_Gui);

	//Coop: leaving the level -- both screens fade, and BOTH players stop. P2
	//used to keep walking around during the fade because only P1 was
	//deactivated here. Paired with the SetActive(true) in CheckMapChange.
	gpBase->mpEffectHandler->GetFade()->FadeOutGlobal(1.5f);

	gpBase->mpPlayer->SetActive(false);
	if(gpBase->mpPlayer2) gpBase->mpPlayer2->SetActive(false);
}

//-----------------------------------------------------------------------

cLuxMap* cLuxMapHandler::LoadMap(const tString& asFileName, bool abLoadEntities)
{
	cLuxMap *pMap = hplNew( cLuxMap, ( FileToMapName(asFileName)) );
	
	pMap->LoadFromFile(msMapFolder+asFileName, abLoadEntities);

	mlstMaps.push_back(pMap);

	return pMap;
}
//-----------------------------------------------------------------------

void cLuxMapHandler::DestroyMap(cLuxMap* apMap, bool abLoadingSaveGame)
{
	////////////////////////////////
	//If the map do me destroyed is current, make sure it is not current
	if(mpCurrentMap == apMap) SetCurrentMap(NULL, abLoadingSaveGame, false,"");

    STLFindAndDelete(mlstMaps,apMap);
}

//-----------------------------------------------------------------------

void cLuxMapHandler::SetCurrentMap(cLuxMap* apMap, bool abRunScript, bool abFirstTime, const tString& asPlayerPos)
{
	if(mpCurrentMap == apMap) return;

	//////////////////////////////////
	//Unload stuff from previous map
    if(mpCurrentMap)
	{
		// Before leaving the map, null both coop viewports to prevent
		// dangling pointers during cleanup.
		if (mpCoopViewport)
		{
			mpCoopViewport->SetCamera(NULL);
			mpCoopViewport->SetWorld(NULL);
			mpCoopViewport->SetVisible(false);
			mpCoopViewport->SetActive(false);
		}
		if (mpCoopP1Viewport)
		{
			mpCoopP1Viewport->SetCamera(NULL);
			mpCoopP1Viewport->SetWorld(NULL);
			mpCoopP1Viewport->SetVisible(false);
			mpCoopP1Viewport->SetActive(false);
		}

		mpCurrentMap->OnLeave(abRunScript);
		
		//Leave callback for modules
		gpBase->RunModuleMessage(eLuxUpdateableMessage_DestroyWorldEntities, mpCurrentMap);
		gpBase->RunModuleMessage(eLuxUpdateableMessage_OnMapLeave, mpCurrentMap);

		// Player 2 is NOT a registered module, so the broadcasts above skip it.
		// Replay them manually to migrate P2's character body OUT of this map's
		// physics world before the map (and that world) is destroyed. Without
		// this, P2's body becomes a dangling pointer and the next map's first
		// update dereferences freed memory => use-after-free crash on every
		// level transition while coop is active.
		if (gpBase->mpPlayer2)
		{
			gpBase->mpPlayer2->DestroyWorldEntities(mpCurrentMap);
			gpBase->mpPlayer2->OnMapLeave(mpCurrentMap);
		}

		// The collision blocker bodies belong to this map's physics world and
		// die with it -- drop the pointers before that happens.
		AbandonCoopPlayerBlockers();
	}

	mpCurrentMap = apMap;

	//////////////////////////////////
	//Setup stuff for previous map
	if(mpCurrentMap)
	{
		//Enter callback for modules
		gpBase->RunModuleMessage(eLuxUpdateableMessage_CreateWorldEntities, mpCurrentMap);
		gpBase->RunModuleMessage(eLuxUpdateableMessage_OnMapEnter, mpCurrentMap);

		//Set the player position
		mpCurrentMap->PlacePlayerAtStartPos(asPlayerPos);

		//Create an automatic checkpoint
		mpCurrentMap->SetCheckPoint("_auto", asPlayerPos, "");
		
		//Map enter callback
		mpCurrentMap->OnEnter(abRunScript, abFirstTime);

		//Set this as world in viewport
		mpViewport->SetWorld(mpCurrentMap->GetWorld());

		// Activate coop split-screen if flag was set (possibly before map load)
		if (mbCoopActive)
		{
			cVector2f vScreen = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();
			int iScreenW = (int)vScreen.x;
			int iScreenH = (int)vScreen.y;

			cVector2l vP1Pos, vP1Size, vP2Pos, vP2Size;
			GetSplitViewportRects(iScreenW, iScreenH, vP1Pos, vP1Size, vP2Pos, vP2Size);

			if (mfOrigFOV == 0.0f)
			{
				mfOrigFOV = gpBase->mpPlayer->GetBaseFOV();
			}

			// Apply dual monitor window if needed
			if (mlSplitScreenMode == eSplitScreenMode_DualMonitor && !mbDualMonitorWindowActive)
			{
				ApplyDualMonitorWindow(true);
			}

			// Main viewport stays up until BOTH coop viewports are ready again --
			// see the handover at the end of this block.

			// Create/update P1 coop viewport
			if (!mpCoopP1Viewport)
			{
				mpCoopP1Viewport = gpBase->mpEngine->GetScene()->CreateViewport(
					gpBase->mpPlayer->GetCamera(),
					mpCurrentMap->GetWorld(),
					false
				);
				mpCoopP1Viewport->SetPosition(vP1Pos);
				mpCoopP1Viewport->SetSize(vP1Size);
				mpCoopP1Viewport->SetRenderer(gpBase->mpEngine->GetGraphics()->GetRenderer(eRenderer_Main));
				mpCoopP1Viewport->SetVisible(false);
				mpCoopP1Viewport->SetActive(false);
				mpCoopP1Viewport->AddRendererCallback(&mRenderCallback);
		mAvatarVisCallbackP1.mpViewport = mpCoopP1Viewport;
		mpCoopP1Viewport->AddViewportCallback(&mAvatarVisCallbackP1);
				mpCoopP1Viewport->AddGuiSet(gpBase->mpGameDebugSet);
				mpCoopP1Viewport->AddGuiSet(gpBase->mpGameHudSet);
			}
			else
			{
				// Reusing an existing viewport after a map transition: the
				// map-leave block nulled its camera/world and hid it, so restore
				// everything for the new map (otherwise P1's split view goes
				// black after the first level change).
				mpCoopP1Viewport->SetCamera(gpBase->mpPlayer->GetCamera());
				mpCoopP1Viewport->SetWorld(mpCurrentMap->GetWorld());
				mpCoopP1Viewport->SetPosition(vP1Pos);
				mpCoopP1Viewport->SetSize(vP1Size);
				mpCoopP1Viewport->SetVisible(false);
				mpCoopP1Viewport->SetActive(false);
			}

			// Post effects. Called for both the freshly-created and the reused
			// viewport: it is idempotent (the composite is built once) and a reused
			// viewport already holds the pointer, so this just re-asserts it.
			SetupCoopViewportPostEffects(mpCoopP1Viewport, false);

			// Create P2 if needed; otherwise migrate the EXISTING P2 into the
			// new map. P2 is not a registered module, so its world entities
			// (including the character body) must be rebuilt in the new map's
			// physics world — mirroring the CreateWorldEntities/OnMapEnter
			// broadcasts that P1 receives above. This pairs with the
			// DestroyWorldEntities/OnMapLeave done in the map-leave block.
			if (!gpBase->mpPlayer2)
			{
				CreatePlayer2(mpCurrentMap);
			}
			else
			{
				gpBase->mpPlayer2->CreateWorldEntities(mpCurrentMap);
				gpBase->mpPlayer2->OnMapEnter(mpCurrentMap);
				gpBase->mpPlayer2->ReenterCurrentMoveState();
				PlacePlayer2NearPlayer1();
			}

			cLuxPlayer *pP2 = gpBase->mpPlayer2;
			if (pP2)
			{
				if (!mpCoopViewport)
				{
					mpCoopViewport = gpBase->mpEngine->GetScene()->CreateViewport(
						pP2->GetCamera(),
						mpCurrentMap->GetWorld(),
						false
					);
					mpCoopViewport->SetPosition(vP2Pos);
					mpCoopViewport->SetSize(vP2Size);
					mpCoopViewport->SetRenderer(gpBase->mpEngine->GetGraphics()->GetRenderer(eRenderer_Main));
					mpCoopViewport->SetVisible(false);
					mpCoopViewport->SetActive(false);
					mpCoopViewport->AddRendererCallback(&mCoopRenderCallback);
			mAvatarVisCallbackP2.mpViewport = mpCoopViewport;
			mpCoopViewport->AddViewportCallback(&mAvatarVisCallbackP2);

					// Create P2's HUD set if it doesn't exist
					if (!mpCoopHudSet)
					{
						mpCoopHudSet = gpBase->mpEngine->GetGui()->CreateSet("CoopHud", NULL);
						mpCoopHudSet->SetVirtualSize(gpBase->mvHudVirtualSize, -1000, 1000, gpBase->mvHudVirtualOffset);
						mpCoopHudSet->SetDrawPriority(0);
					}
					mpCoopViewport->AddGuiSet(mpCoopHudSet);

					if (!mpCoopDarkOverlayGfx)
					{
						mpCoopDarkOverlayGfx = gpBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1,1), eGuiMaterial_Alpha);
					}
				}
				else
				{
					// Reusing an existing viewport after a map transition:
					// restore camera/world/position/visibility that the
					// map-leave block cleared (otherwise P2's view goes black
					// after the first level change).
					mpCoopViewport->SetCamera(pP2->GetCamera());
					mpCoopViewport->SetWorld(mpCurrentMap->GetWorld());
					mpCoopViewport->SetPosition(vP2Pos);
					mpCoopViewport->SetSize(vP2Size);
					mpCoopViewport->SetVisible(false);
					mpCoopViewport->SetActive(false);
				}

				// Post effects — see the P1 call above.
				SetupCoopViewportPostEffects(mpCoopViewport, true);
			}

			////////////////////////////////////////////////////////////////////
			// The handover, in one step -- same reason as the one in SetCoopMode.
			//
			// The map-leave block above nulls BOTH coop viewports' camera and world.
			// This block put them back, but it hid the main viewport first, restored
			// P1's and showed it, and only THEN created or migrated P2 -- which loads
			// models, entities and sounds and takes real time. Every frame drawn in
			// that window had P1's half rendering and P2's viewport still holding a
			// null camera and world, so cScene::Render skipped it entirely and P2's
			// half kept the black clear.
			//
			// This path runs on every level change AND on every death or checkpoint
			// reload, which is why P2's viewport goes completely black around dying.
			//BOTH must be genuinely renderable, or nothing changes.
			//
			// cScene::Render only draws a viewport's world when it has a renderer, a
			// world AND a camera. Miss any one and it silently draws nothing, leaving
			// that rect as the black clear -- which is exactly what a screenshot of this
			// bug looks like: P1's half lit and correct, P2's half pure black.
			//
			// So the main viewport is only given up once both halves can actually draw.
			// If P2 is not ready yet, keeping one full-screen view is wrong for co-op but
			// it is a WHOLE picture; trading it for one live half and one black half is
			// strictly worse. The next tick through here fixes it up.
			const bool bP1Ready = mpCoopP1Viewport && mpCoopP1Viewport->GetCamera() &&
								  mpCoopP1Viewport->GetWorld();
			const bool bP2Ready = mpCoopViewport   && mpCoopViewport->GetCamera()   &&
								  mpCoopViewport->GetWorld();

			if(bP1Ready && bP2Ready)
			{
				mpViewport->SetActive(false);
				mpViewport->SetVisible(false);

				mpCoopP1Viewport->SetVisible(true);
				mpCoopP1Viewport->SetActive(true);

				mpCoopViewport->SetVisible(true);
				mpCoopViewport->SetActive(true);
			}
		}
		else if (mpCoopViewport)
		{
			mpCoopViewport->SetWorld(mpCurrentMap->GetWorld());
		}

		mRenderCallback.mpPhysicsWorld = mpCurrentMap->GetPhysicsWorld();
		mRenderCallback.mpLowLevelGfx = gpBase->mpEngine->GetGraphics()->GetLowLevel();
	}
	else
	{
		// Null out P2's viewport camera BEFORE destroying P2,
		// so the viewport doesn't hold a dangling camera pointer.
		if (mpCoopViewport)
		{
			mpCoopViewport->SetCamera(NULL);
			mpCoopViewport->SetVisible(false);
			mpCoopViewport->SetActive(false);
			mpCoopViewport->SetWorld(NULL);
		}

		// Also hide P1 coop viewport
		if (mpCoopP1Viewport)
		{
			mpCoopP1Viewport->SetCamera(NULL);
			mpCoopP1Viewport->SetWorld(NULL);
			mpCoopP1Viewport->SetVisible(false);
			mpCoopP1Viewport->SetActive(false);
		}

		// Destroy P2 before clearing world
		if (gpBase->mpPlayer2) DestroyPlayer2(mpCurrentMap);

		//If no map, set NULL as world
		mpViewport->SetWorld(NULL);
	}
}

//-----------------------------------------------------------------------

void cLuxMapHandler::PauseSoundsAndMusic()
{
	if(mbPausedSoundsAndMusic) return;

	cSound *pSound = gpBase->mpEngine->GetSound();
	pSound->GetSoundHandler()->PauseAll(eSoundEntryType_All);
	pSound->GetMusicHandler()->Pause();
	mbPausedSoundsAndMusic = true;
}

void cLuxMapHandler::ResumeSoundsAndMusic()
{
	if(mbPausedSoundsAndMusic)
	{
		cSound *pSound = gpBase->mpEngine->GetSound();
		pSound->GetSoundHandler()->ResumeAll(eSoundEntryType_All);
		pSound->GetMusicHandler()->Resume();
		mbPausedSoundsAndMusic = false;
	}
}

//-----------------------------------------------------------------------

void cLuxMapHandler::ClearSaveMapCollection()
{
	mpSavedGameMutex->Lock();
		mpSavedGame->Reset();
	mpSavedGameMutex->Unlock();
}

//-----------------------------------------------------------------------

void cLuxMapHandler::SetSavedMapCollection(cLuxSavedGameMapCollection *apMaps)
{
	mpSavedGameMutex->Lock();
	{
		hplDelete(mpSavedGame);

		mpSavedGame = apMaps;
	}
	mpSavedGameMutex->Unlock();
}

//-----------------------------------------------------------------------

void cLuxMapHandler::AppLostInputFocus()
{
	PauseSoundsAndMusic();
}

//-----------------------------------------------------------------------

void cLuxMapHandler::AppGotInputFocus()
{
	ResumeSoundsAndMusic();
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMapHandler::LoadMainConfig()
{
	mpPostEffect_Bloom->SetDisabled(gpBase->mpMainConfig->GetBool("Graphics", "PostEffectBloom", true)==false);
	mpPostEffect_ImageTrail->SetDisabled(gpBase->mpMainConfig->GetBool("Graphics", "PostEffectImageTrail", true)==false);
	mpPostEffect_Sepia->SetDisabled(gpBase->mpMainConfig->GetBool("Graphics", "PostEffectSepia", true)==false);
	mpPostEffect_RadialBlur->SetDisabled(gpBase->mpMainConfig->GetBool("Graphics", "PostEffectRadialBlur", true)==false);

	cRenderSettings *pRenderSettings = mpViewport->GetRenderSettings();
	pRenderSettings->mbUseEdgeSmooth = gpBase->mpConfigHandler->mbEdgeSmooth; //This is saved in config handler!
}

void cLuxMapHandler::SaveMainConfig()
{
	gpBase->mpMainConfig->SetBool("Graphics", "PostEffectBloom", mpPostEffect_Bloom->IsDisabled()==false);
	gpBase->mpMainConfig->SetBool("Graphics", "PostEffectImageTrail", mpPostEffect_ImageTrail->IsDisabled()==false);
	gpBase->mpMainConfig->SetBool("Graphics", "PostEffectSepia", mpPostEffect_Sepia->IsDisabled()==false);
	gpBase->mpMainConfig->SetBool("Graphics", "PostEffectRadialBlur", mpPostEffect_RadialBlur->IsDisabled()==false);
}

//-----------------------------------------------------------------------

tString cLuxMapHandler::FileToMapName(const tString& asFile)
{
	return cString::ToLowerCase(cString::GetFileName(cString::SetFileExt(asFile, "")));
}

//-----------------------------------------------------------------------

void cLuxMapHandler::SetShowCommentary(bool abX)
{
	mbShowCommentary = abX;

	if(mbShowCommentary==false)
	{
		gpBase->mpEffectHandler->GetPlayCommentary()->Stop();
	}
}

//-----------------------------------------------------------------------

void cLuxMapHandler::CheckMapChange(float afTimeStep)
{
	if(mMapChangeData.mbActive==false) return;
	if(gpBase->mpEffectHandler->GetFade()->IsFading()) return;

	///////////////////////////////////////
	// Setup variables
    float fTimeTaken =0;

	///////////////////////////////////////
	// Fade out and disable player
	mMapChangeData.mbActive = false;
	//Coop: arriving in the new level -- both screens, both players. Runs before
	//the load, so an aborted map change cannot leave anyone frozen.
	gpBase->mpEffectHandler->GetFade()->FadeInGlobal(2.0f);
	gpBase->mpPlayer->SetActive(true);
	if(gpBase->mpPlayer2) gpBase->mpPlayer2->SetActive(true);

	///////////////////////////////////////
	// Write pending savegame queries
	cLuxSaveHandlerThreadClass* pThreadClass = gpBase->mpSaveHandler->GetThreadClass();
	if(pThreadClass->IsRunning())
		pThreadClass->ProcessPendingSaves();

	//////////////////////////////////
	//Clean up
	// Must do this before OnEnter!
	gpBase->mpHelpFuncs->CleanupData();

	///////////////////////
	// Load map
	tString sNewMapName = FileToMapName(mMapChangeData.msMapFile);
	if(mpCurrentMap->GetName() != sNewMapName)
	{
		mpSavedGameMutex->Lock();

		//////////////////////
		// Run onleave before saving!
		mpCurrentMap->RunScript("OnLeave()");//since script is not run in SetCurrenMap

		///////////////////////////////////////
		// Draw loading screen
		unsigned long lLoadStartTime = cPlatform::GetApplicationTime();
		gpBase->mpLoadScreenHandler->DrawGameScreen();

		//////////////////////
		// Save old map
		mpSavedGame->SaveMap(mpCurrentMap);

		//////////////////////
		// Fadeout sounds and disable stop (meaning they will fade even when sound entities are destroyed)
		gpBase->mpEngine->GetSound()->GetSoundHandler()->FadeOutAll(eSoundEntryType_World, 0.5f, true);
		
		//////////////////////
		// Load new map
		cLuxMap *pLastMap = mpCurrentMap;
		cLuxMap *pMap = LoadMap(mMapChangeData.msMapFile,true);
		if(pMap == NULL)
		{
			Error("Could not load map '%s'!\n", mMapChangeData.msMapFile.c_str());
			return;
		}
		
		if(pLastMap)
		{
			if (pLastMap->GetName() == "08_cellar_maze" && pMap->GetName() == "09_back_hall")
			{
				gpBase->mpAchievementHandler->UnlockAchievement(eLuxAchievement_EscapeArtist);
			}

			if (pLastMap->GetName() == "14_elevator" && pMap->GetName() == "15_prison_south")
			{
				gpBase->mpAchievementHandler->UnlockAchievement(eLuxAchievement_Descendant);
			}

			//////////////
			// HARDMODE
			if (gpBase->mbHardMode &&
				pLastMap->GetName() == "27_torture_chancel_redux" && pMap->GetName() == "28_inner_sanctum")
			{
				gpBase->mpPlayer->AddSanity(100.f, true);
			}
		}

		//////////////////////
		// Set new and destroy old
		bool bFirstTime = mpSavedGame->MapExists(sNewMapName)==false;	
		
		SetCurrentMap(pMap, false, bFirstTime, mMapChangeData.msStartPos);
		DestroyMap(pLastMap, false);

		//////////////////////
		// Load new map data
		mpSavedGame->LoadMap(mpCurrentMap);

		//////////////////////
		// Run enter script! (otherwise a save in oneter will not be correct!)
		if(bFirstTime) mpCurrentMap->RunScript("OnStart()");
		mpCurrentMap->RunScript("OnEnter()");


		mpSavedGameMutex->Unlock();

		//////////////////////////////////
		//Check if any more load time needed
		fTimeTaken = (float)(cPlatform::GetApplicationTime() - lLoadStartTime)/1000.0f;
		
		ProgLog(eLuxProgressLogLevel_High, "Entering map "+ mpCurrentMap->GetName());
	}
	///////////////////////
	// Map already loaded.
	else
	{
		mpCurrentMap->PlacePlayerAtStartPos(mMapChangeData.msStartPos);
	}

	//////////////////////////////////
	// Check if text should be left on a bit longer
	if(fTimeTaken>0)
	{
		gpBase->mpLoadScreenHandler->GameScreenLoadDone(mMapChangeData.msSound, fTimeTaken);
	}
	else
	{
		//Play finished sound
		gpBase->mpHelpFuncs->PlayGuiSoundData(mMapChangeData.msSound, eSoundEntryType_Gui);

	}
}
 
//-----------------------------------------------------------------------