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

#include "scene/Scene.h"

#include "scene/Viewport.h"
#include "scene/Camera.h"
#include "scene/World.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/Script.h"
#include "system/Platform.h"

#include "resources/Resources.h"
#include "resources/ScriptManager.h"
#include "resources/FileSearcher.h"
#include "resources/WorldLoaderHandler.h"

#include "graphics/Graphics.h"
#include "graphics/Renderer.h"
#include "graphics/PostEffectComposite.h"
#include "graphics/LowLevelGraphics.h"

#include "sound/Sound.h"
#include "sound/LowLevelSound.h"
#include "sound/SoundHandler.h"

#include "gui/Gui.h"
#include "gui/GuiSet.h"

#include "physics/Physics.h"

#include "impl/ImGuiDebugMenu.h"

#include <chrono>


namespace hpl {

	//Latched for the black frame probe over in cLowLevelGraphicsSDL::SwapBuffers. That
	//runs after cScene::Render has returned and after ImGui has drawn, so it cannot see
	//any of this for itself.
	int gLuxDbgVisibleViewports = 0;
	int gLuxDbgWorldRenders = 0;
	int gLuxDbgFramesDropped = 0;

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cScene::cScene(cGraphics *apGraphics,cResources *apResources, cSound* apSound,cPhysics *apPhysics,
					cSystem *apSystem, cAI *apAI,cGui *apGui, cHaptic *apHaptic)
		: iUpdateable("HPL_Scene")
	{
		mpGraphics = apGraphics;
		mpResources = apResources;
		mpSound = apSound;
		mpPhysics = apPhysics;
		mpSystem = apSystem;
		mpAI = apAI;
		mpGui = apGui;
		mpHaptic = apHaptic;

		mpCurrentListener = NULL;

		//Nothing has been drawn yet, so the first frame is always presentable and there
		//is no previous world render to compare against.
		mlHeldBlackFrames = 0;
	}

	//-----------------------------------------------------------------------

	cScene::~cScene()
	{
		Log("Exiting Scene Module\n");
		Log("--------------------------------------------------------\n");

		STLDeleteAll(mlstViewports);
		STLDeleteAll(mlstWorlds);
		STLDeleteAll(mlstCameras);

		Log("--------------------------------------------------------\n\n");

	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	cViewport* cScene::CreateViewport(cCamera *apCamera, cWorld *apWorld, bool abPushFront)
	{
		cViewport *pViewport = hplNew ( cViewport, (this) );	

		pViewport->SetCamera(apCamera);
		pViewport->SetWorld(apWorld);
		pViewport->SetSize(-1);
		pViewport->SetRenderer(mpGraphics->GetRenderer(eRenderer_Main));

		if (abPushFront) {
			mlstViewports.push_front(pViewport);
		} else {
			mlstViewports.push_back(pViewport);
		}

		return pViewport;
	}
	
	//-----------------------------------------------------------------------

	void cScene::DestroyViewport(cViewport* apViewPort)
	{
		STLFindAndDelete(mlstViewports, apViewPort);
	}

	//-----------------------------------------------------------------------

	bool cScene::ViewportExists(cViewport* apViewPort)
	{
		for(tViewportListIt it = mlstViewports.begin(); it != mlstViewports.end(); ++it)
		{
			if(apViewPort == *it) return true;
		}

		return false;
	}

	//-----------------------------------------------------------------------

	void cScene::SetCurrentListener(cViewport* apViewPort)
	{
		//If there was a previous listener make sure that world is not a listener.
		if(mpCurrentListener != NULL && ViewportExists(mpCurrentListener))
		{
			mpCurrentListener->SetIsListener(false);
			cWorld *pWorld = mpCurrentListener->GetWorld();
			if(pWorld && WorldExists(pWorld)) pWorld->SetIsSoundEmitter(false);
		}
		
		mpCurrentListener = apViewPort;
		if(mpCurrentListener)
		{
			mpCurrentListener->SetIsListener(true);
			cWorld *pWorld = mpCurrentListener->GetWorld();
			if(pWorld) pWorld->SetIsSoundEmitter(true);
		}
	}

	//-----------------------------------------------------------------------

	cCamera* cScene::CreateCamera(eCameraMoveMode aMoveMode)
	{
		cCamera *pCamera = hplNew( cCamera, () );
		pCamera->SetAspect(mpGraphics->GetLowLevel()->GetScreenSizeFloat().x /
							mpGraphics->GetLowLevel()->GetScreenSizeFloat().y);

		//Add Camera to list
		mlstCameras.push_back(pCamera);

		return pCamera;
	}


	//-----------------------------------------------------------------------

	void cScene::DestroyCamera(cCamera* apCam)
	{
		STLFindAndDelete(mlstCameras, apCam);
	}

	//-----------------------------------------------------------------------

	void cScene::Render(float afFrameTime, tFlag alFlags)
	{
		//Increase the frame count (do this at top, so render count is valid until this Render is called again!)
		iRenderer::IncRenderFrameCount();

		///////////////////////////////////////
		// DIAGNOSTIC: how many viewports are we really drawing?
		// The loop below skips on IsVisible() ALONE -- it never looks at
		// IsActive() -- so any path that clears one without the other leaves a
		// fully rendering viewport behind. The main viewport is created first,
		// so it renders UNDERNEATH the split views and is invisible when that
		// happens, while still costing a whole extra deferred pass per frame.
		// Expect 1 in single player and 2 in split-screen co-op. A 3 is the bug.
		int lVisibleViewports = 0;
		cViewport *pOnlyVisibleViewport = NULL;

		////////////////////////////////////////////////////////////////////////
		// Of the visible ones, how many will actually PUT SOMETHING DOWN.
		//
		// A viewport owning a renderer but missing its world, camera or frustum
		// draws nothing at all, and the clear below has already blanked the
		// buffer for it. Counted separately here so the clear can be told about
		// it; see the note where it is used.
		//
		// A viewport with NO renderer is not dead -- that is a GUI-only overlay
		// (the dual-monitor menu mirror, the load screen) and it never had a
		// world to draw in the first place.
		int lDrawableViewports = 0;
		int lDeadViewports = 0;
		{
			tViewportListIt countIt = mlstViewports.begin();
			for(; countIt != mlstViewports.end(); ++countIt)
			{
				cViewport *pCountVp = *countIt;
				if(pCountVp->IsVisible()==false) continue;

				++lVisibleViewports;
				pOnlyVisibleViewport = pCountVp;	//meaningful only when the count is 1

				if(pCountVp->GetRenderer())
				{
					cCamera *pCountCam = pCountVp->GetCamera();
					if(pCountVp->GetWorld() && pCountCam && pCountCam->GetFrustum())
						++lDrawableViewports;
					else
						++lDeadViewports;
				}
			}
			::ImGuiDebugMenu::SetDiagViewportCounts(lVisibleViewports, (int)mlstViewports.size());
			::ImGuiDebugMenu::LatchDiagFrameCounters();
		}


		///////////////////////////////////////////
		// Clear the back buffer unless one visible viewport genuinely covers it.
		//
		// Areas outside the split-screen viewports have to be black rather than a stale
		// frame, so the clear cannot simply go away. But clearing when nothing then
		// overwrites it turns a repeat frame into a black flash, so it cannot be
		// unconditional either.
		//
		// The test has to be COVERAGE, not the viewport count. Testing the count alone
		// assumed a single visible viewport must be either full-screen or a transient
		// GUI-only one. In co-op that is false: cLuxMapHandler::RestoreBackgroundCapture
		// leaves exactly ONE viewport visible while an inventory or journal is open, and
		// that viewport still holds its HALF-SCREEN split rect. The clear was suppressed,
		// nothing wrote the other half of the back buffer, and with double buffering that
		// half alternated between two stale buffers -- one of them the black-cleared
		// capture frame. Flicker, then black, for exactly as long as the menu was open.
		// Single player never reproduced it because there the one viewport really does
		// cover the screen.
		bool bClearBackBuffer = true;
		if(lVisibleViewports == 1 && pOnlyVisibleViewport)
		{
			const cVector2l vPos = pOnlyVisibleViewport->GetPosition();
			const cVector2l vSize = pOnlyVisibleViewport->GetSize();

			const cVector2l vScreen = mpGraphics->GetLowLevel()->GetScreenSizeInt();

			//A negative size means "whatever the framebuffer is", i.e. all of it.
			const bool bCovers =	(vSize.x < 0 && vSize.y < 0) ||
									(vSize.x >= vScreen.x && vSize.y >= vScreen.y);

			if(vPos == cVector2l(0,0) && bCovers) bClearBackBuffer = false;
		}

		///////////////////////////////////////////
		// There WAS a rule here that held the clear back whenever a visible
		// viewport had no world to draw, on the theory that it was mid-transition
		// and its rect would otherwise flash black. It is gone, and it was wrong.
		//
		// cScene::CreateViewport hands every viewport the main renderer and a NULL
		// world, so a GUI-only viewport -- the journal's, the inventory's, the load
		// screen's -- looks exactly like a viewport that failed to draw. They are
		// worldless on purpose and permanently, so the rule fired for the whole
		// time any menu was open and suppressed the clear four frames in five.
		// Nothing distinguishes "has no world yet" from "has no world ever" at this
		// level, which means the test cannot be written here at all.
		mlHeldBlackFrames = 0;

		mpGraphics->GetLowLevel()->SetCurrentFrameBuffer(NULL);

		//////////////////////////////////////////////////////////////////
		// Force the scissor rectangle genuinely OFF before the clear below.
		//
		// That clear paints the whole back buffer black on the assumption that a
		// full-screen viewport overwrites it. glClear obeys GL_SCISSOR_TEST, and so
		// does every draw after it, so a scissor left switched on over a small area
		// does not merely clip something -- it leaves the black standing across the
		// rest of the screen. That is a black FRAME, not a black corner.
		//
		// Scissor enable is cached in two places that can disagree --
		// iRenderFunctions::mbCurrentScissorActive and
		// cLowLevelGraphicsSDL::mbScissorActive -- and BOTH early-out when asked for
		// the value they think they already hold, so once they drift apart a plain
		// SetScissorActive(false) can be swallowed and never reach GL at all.
		//
		// Hence the pair rather than a single call: whatever the low-level cache
		// currently believes, the first call makes it believe 'on' and the second
		// therefore has to issue a real glDisable. All four combinations of
		// (cache, actual GL) end with scissor genuinely off and the cache agreeing.
		// One redundant enable/disable pair per frame; it costs nothing and it makes
		// a whole class of black frame impossible instead of merely unlikely.
		mpGraphics->GetLowLevel()->SetScissorActive(true);
		mpGraphics->GetLowLevel()->SetScissorActive(false);

		if(bClearBackBuffer)
		{
			mpGraphics->GetLowLevel()->SetClearColor(cColor(0, 0, 0, 1));
			mpGraphics->GetLowLevel()->ClearFrameBuffer(eClearFrameBufferFlag_Color);
		}

		///////////////////////////////////////////
		// Iterate all viewports and render
		//
		// lWorldRenders counts the viewports that actually put a world on screen this
		// frame -- not the ones that are merely visible. A frame where that ends up at
		// zero is the one that flashes black; see the note after the loop.
		int lWorldRenders = 0;
		int lViewportIndex = -1;

		tViewportListIt viewIt = mlstViewports.begin();
		for(; viewIt != mlstViewports.end(); ++viewIt)
		{
			cViewport *pViewPort = *viewIt;
			++lViewportIndex;
			if(pViewPort->IsVisible()==false) continue;

			//////////////////////////////////////////////
			//Init vars
			cPostEffectComposite *pPostEffectComposite = pViewPort->GetPostEffectComposite();
			bool bPostEffects = false;
			iRenderer *pRenderer = pViewPort->GetRenderer();
			cCamera *pCamera = pViewPort->GetCamera();
			cFrustum *pFrustum = pCamera ? pCamera->GetFrustum() : NULL;

			//////////////////////////////////////////////
			//Render world and call callbacks
			if(alFlags & tSceneRenderFlag_World)
			{
				pViewPort->RunViewportCallbackMessage(eViewportMessage_OnPreWorldDraw);
				
				if(pPostEffectComposite && (alFlags & tSceneRenderFlag_PostEffects)) 
				{
					bPostEffects = pPostEffectComposite->HasActiveEffects();
				}
				
				if(pRenderer && pViewPort->GetWorld() && pFrustum)
				{
					START_TIMING(RenderWorld)
					// DIAGNOSTIC: the whole world render, per viewport, summed per frame.
					// cScene::Render runs OUTSIDE cUpdater, so without this the entire
					// rendering cost was missing from the profiler.
					std::chrono::high_resolution_clock::time_point profStart = std::chrono::high_resolution_clock::now();
					pRenderer->Render(	afFrameTime,pFrustum,
										pViewPort->GetWorld(),pViewPort->GetRenderSettings(), 
										pViewPort->GetRenderTarget(),
										bPostEffects,
										pViewPort->GetRendererCallbackList());
					std::chrono::duration<float, std::milli> profMs = std::chrono::high_resolution_clock::now() - profStart;
					::ImGuiDebugMenu::AddDiagModuleTime("** RenderWorld", profMs.count());
					STOP_TIMING(RenderWorld)

					++lWorldRenders;
				}
				else
				{
					////////////////////////////////////////////////////////////
					// THIS is a black half-screen, and it is where the flicker
					// lives.
					//
					// The viewport is visible, so the frame was cleared to black
					// for it, but one of renderer / world / camera came back NULL
					// so no world is drawn into it -- and the GUI still is, which
					// is why these frames show the HUD floating on nothing.
					//
					// Recorded rather than merely counted: which viewport, and
					// which of the four went missing. They look identical on
					// screen and have nothing else in common.
					::ImGuiDebugMenu::ReportBlackViewportFrame(
							lViewportIndex,
							pRenderer != NULL,
							pViewPort->GetWorld() != NULL,
							pCamera != NULL,
							pFrustum != NULL,
							iRenderer::GetRenderFrameCount());

					//If no renderer sets up viewport do that by our selves.
					cRenderTarget* pRenderTarget = pViewPort->GetRenderTarget();
					mpGraphics->GetLowLevel()->SetCurrentFrameBuffer(	pRenderTarget->mpFrameBuffer,
																		pRenderTarget->mvPos,
																		pRenderTarget->mvSize);
				}
				pViewPort->RunViewportCallbackMessage(eViewportMessage_OnPostWorldDraw);

				//////////////////////////////////////////////
				//Render 3D GuiSets
				// Should this really be here? Or perhaps send in a frame buffer depending on the renderer.
				START_TIMING(Render3DGui)
				Render3DGui(pViewPort,pFrustum, afFrameTime);
				STOP_TIMING(Render3DGui)
			}

			//////////////////////////////////////////////
			//Render Post effects
			if(bPostEffects)
			{
				//TODO: If renderer is null get texture from frame buffer and if frame buffer is NULL, then copy to a texture.
				//		Or this is solved?
				iTexture *pInputTexture = pRenderer->GetPostEffectTexture();

				START_TIMING(RenderPostEffects)
				std::chrono::high_resolution_clock::time_point profPost = std::chrono::high_resolution_clock::now();
				pPostEffectComposite->Render(afFrameTime, pFrustum, pInputTexture,pViewPort->GetRenderTarget());
				std::chrono::duration<float, std::milli> profPostMs = std::chrono::high_resolution_clock::now() - profPost;
				::ImGuiDebugMenu::AddDiagModuleTime("** PostEffects", profPostMs.count());
				STOP_TIMING(RenderPostEffects)
			}
			
			//////////////////////////////////////////////
			//Render Screen GUI
			if(alFlags & tSceneRenderFlag_Gui)
			{
				START_TIMING(RenderGUI)
				std::chrono::high_resolution_clock::time_point profGui = std::chrono::high_resolution_clock::now();
				RenderScreenGui(pViewPort, afFrameTime);
				std::chrono::duration<float, std::milli> profGuiMs = std::chrono::high_resolution_clock::now() - profGui;
				::ImGuiDebugMenu::AddDiagModuleTime("** RenderGUI", profGuiMs.count());
				STOP_TIMING(RenderGUI)
			}
		}
		gLuxDbgVisibleViewports = lVisibleViewports;
		gLuxDbgWorldRenders = lWorldRenders;
	}

	//-----------------------------------------------------------------------

	void cScene::PostUpdate(float afTimeStep)
	{
		//////////////////////////////////////
		//Update worlds
		tWorldListIt it = mlstWorlds.begin();
		for(; it != mlstWorlds.end(); ++it)
		{
			cWorld *pWorld = *it;
            if(pWorld->IsActive()) pWorld->Update(afTimeStep);
		}


		//////////////////////////////////////
		//Update listener position with current listener, if there is one.
		if(mpCurrentListener && mpCurrentListener->GetCamera())
		{
			cCamera* pCamera3D = mpCurrentListener->GetCamera();
			mpSound->GetLowLevel()->SetListenerAttributes(	pCamera3D->GetPosition(), cVector3f(0,0,0),
															pCamera3D->GetForward()*-1.0f, pCamera3D->GetUp());
		}
	}

	//-----------------------------------------------------------------------

	void cScene::Reset()
	{
	}

	//-----------------------------------------------------------------------

	cWorld* cScene::LoadWorld(const tString& asFile, tWorldLoadFlag aFlags)
	{
		///////////////////////////////////
		// Load the map file
		tWString asPath = mpResources->GetFileSearcher()->GetFilePath(asFile);
		if(asPath == _W(""))
		{
			if(cResources::GetCreateAndLoadCompressedMaps())
				asPath = mpResources->GetFileSearcher()->GetFilePath(cString::SetFileExt(asFile,"cmap"));
			
			if(asPath == _W(""))
			{
				Error("World '%s' doesn't exist\n",asFile.c_str());
				return NULL;
			}
		}

		cWorld* pWorld = mpResources->GetWorldLoaderHandler()->LoadWorld(asPath, aFlags);
		if(pWorld==NULL){
			Error("Couldn't load world from '%s'\n",cString::To8Char(asPath).c_str());
			return NULL;
		}

		return pWorld;
	}

	//-----------------------------------------------------------------------

	cWorld* cScene::CreateWorld(const tString& asName)
	{
		cWorld* pWorld = hplNew( cWorld, (asName,mpGraphics,mpResources,mpSound,mpPhysics,this,
										mpSystem,mpAI,mpHaptic) );

		mlstWorlds.push_back(pWorld);

		return pWorld;
	}

	//-----------------------------------------------------------------------

	void cScene::DestroyWorld(cWorld* apWorld)
	{
		STLFindAndDelete(mlstWorlds,apWorld);
	}

	//-----------------------------------------------------------------------

	bool cScene::WorldExists(cWorld* apWorld)
	{
		for(tWorldListIt it = mlstWorlds.begin(); it != mlstWorlds.end(); ++it)
		{
			if(apWorld == *it) return true;
		}

		return false;
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cScene::Render3DGui(cViewport *apViewPort,cFrustum *apFrustum,float afTimeStep)
	{
		if(apViewPort->GetCamera()==NULL) return;

		cGuiSetListIterator it = apViewPort->GetGuiSetIterator();	
		while(it.HasNext())
		{
			cGuiSet *pSet = it.Next();
			if(pSet->Is3D())
			{
				pSet->Render(apFrustum);
			}
		}
	}
	
	void cScene::RenderScreenGui(cViewport *apViewPort,float afTimeStep)
	{
		///////////////////////////////////////
		//Put all of the non 3D sets in to a sorted map
		typedef std::multimap<int, cGuiSet*> tPrioMap;
		tPrioMap mapSortedSets;

        cGuiSetListIterator it = apViewPort->GetGuiSetIterator();	
		while(it.HasNext())
		{
			cGuiSet *pSet = it.Next();
			
			if(pSet->Is3D()==false)
				mapSortedSets.insert(tPrioMap::value_type(pSet->GetDrawPriority(),pSet));
		}

		///////////////////////////////////////
		//Iterate and render all sets
		if(mapSortedSets.empty()) return;

		//////////////////////////////////////////////////////////////////////
		// Bind this viewport's real target EXPLICITLY before any GUI renders.
		//
		// cGuiSet::Render never binds a framebuffer -- it draws into whatever the
		// world render and the post-effect chain happened to leave bound. On the
		// ordinary path that is the back buffer at the viewport's rect, so it went
		// unnoticed. But several paths exit with an INTERNAL FBO still bound (a
		// reflection pass early-returns CopyToFrameBuffer, a post effect that
		// deactivates mid-frame, the no-renderer viewport branch) -- and then the
		// whole GUI pass for this viewport lands in an offscreen buffer and simply
		// never appears on screen for that frame, while the world (already
		// composited) does. That is "the logo and text flicker but the background
		// is fine", per viewport, per frame, invisible to any probe that reads the
		// back buffer -- the GUI is not dark in the back buffer, it is absent.
		//
		// One bind + viewport rect per viewport per frame; identity when the state
		// was already correct.
		{
			cRenderTarget *pRT = apViewPort->GetRenderTarget();
			mpGraphics->GetLowLevel()->SetCurrentFrameBuffer(	pRT->mpFrameBuffer,
																pRT->mvPos, pRT->mvSize);
		}

		tPrioMap::iterator SortIt = mapSortedSets.begin();
		for(; SortIt != mapSortedSets.end(); ++SortIt)
		{
			cGuiSet *pSet = SortIt->second;

			//Log("Rendering gui '%s'\n", pSet->GetName().c_str());

			pSet->Render(NULL);
		}
	}

	//-----------------------------------------------------------------------
}
