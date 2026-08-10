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

#include "LuxEffectHandler.h"

#include "system/Platform.h"

#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxHelpFuncs.h"
#include "LuxMessageHandler.h"
#include "LuxCommentaryIcon.h"

/**
 * Coop: the player who is NOT currently being acted on, or NULL if there is
 * only one. Acting as P2 means gpBase->mpPlayer IS P2, so the other one is
 * whoever got swapped out.
 */
static cLuxPlayer* CoopGetOtherPlayerForEffects()
{
	cLuxPlayer *pOther = (gpBase->mpPlayer == gpBase->mpPlayer2) ?
							cLuxPlayer::CoopGetSwappedOutPlayer() : gpBase->mpPlayer2;

	if(pOther == NULL || pOther == gpBase->mpPlayer) return NULL;

	return pOther;
}


//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffectHandler::cLuxEffectHandler() : iLuxUpdateable("LuxEffectHandler")
{
	mpFade = hplNew( cLuxEffect_Fade, () );
	mvEffects.push_back(mpFade);

	mpFlash = hplNew( cLuxEffect_Flash, () );
	mvEffects.push_back(mpFlash);

	mpSanityGainFlash = hplNew( cLuxEffect_SanityGainFlash, () );
	mvEffects.push_back(mpSanityGainFlash);

	mpPlayVoice = hplNew( cLuxEffect_PlayVoice, () );
	mvEffects.push_back(mpPlayVoice);

	mpImageTrail = hplNew( cLuxEffect_ImageTrail, () );
	mvEffects.push_back(mpImageTrail);

	mpScreenShake = hplNew( cLuxEffect_ShakeScreen, () );
	mvEffects.push_back(mpScreenShake);

	mpSepiaColor = hplNew( cLuxEffect_SepiaColor, () );
	mvEffects.push_back(mpSepiaColor);

	mpRadialBlur = hplNew( cLuxEffect_RadialBlur, () );
	mvEffects.push_back(mpRadialBlur);

	mpEmotionFlash = hplNew( cLuxEffect_EmotionFlash, () );
	mvEffects.push_back(mpEmotionFlash);

	mpPlayCommentary = hplNew( cLuxEffect_PlayCommentary, () );
	mvEffects.push_back(mpPlayCommentary);
}

//-----------------------------------------------------------------------

cLuxEffectHandler::~cLuxEffectHandler()
{
	STLDeleteAll(mvEffects);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PLAY COMMENTARY
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------


cLuxEffect_PlayCommentary::cLuxEffect_PlayCommentary()
{
	mpSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	mpMusicHandler = gpBase->mpEngine->GetSound()->GetMusicHandler();

	mpSoundEntry = NULL;
	mlSoundEntryID = -1;
	Reset();
}

cLuxEffect_PlayCommentary::~cLuxEffect_PlayCommentary()
{
	Reset();
}

//-----------------------------------------------------------------------

void cLuxEffect_PlayCommentary::Start(const tString &asTalker,const tString &asTopic, const tString &asFile, int alIconId)
{
	if(mpSoundEntry)
	{
		Stop();

		//Set the icon as not playing
		if(gpBase->mpMapHandler->GetCurrentMap())
		{
			iLuxEntity *pEntity = gpBase->mpMapHandler->GetCurrentMap()->GetEntityByID(mlIconID, eLuxEntityType_CommentaryIcon);
			if(pEntity)
			{
				cLuxCommentaryIcon *mpIcon = static_cast<cLuxCommentaryIcon*>(pEntity);
				mpIcon->SetPlayingSound(false);
			}
		}
	}
	
	msTalker = asTalker;
	msTopic  = asTopic;
	mlIconID = alIconId;

	mpSoundEntry = mpSoundHandler->PlayGuiStream(asFile,false, 1.0f);
	if(mpSoundEntry)
	{
		mlSoundEntryID = mpSoundEntry->GetId();
		SetActive(true);
		
		mpSoundHandler->FadeGlobalVolume(0.15f,0.5f,  eSoundEntryType_World, eLuxGlobalVolumeType_Commentary, false);
		mpMusicHandler->FadeVolumeMul(0.15f, 0.5f);
		gpBase->mpEffectHandler->GetPlayVoice()->SetVolumeMul(0.1f);
	}
	
}

//-----------------------------------------------------------------------

void cLuxEffect_PlayCommentary::Stop()
{
	if(mbActive==false) return;

	if(mpSoundHandler->IsValid(mpSoundEntry,mlSoundEntryID)) mpSoundEntry->FadeOut(1);
	mpSoundEntry = NULL;

	mpSoundHandler->FadeGlobalVolume(1.0f,0.5f,  eSoundEntryType_World, eLuxGlobalVolumeType_Commentary, false);
	mpMusicHandler->FadeVolumeMul(1.0f, 0.5f);
	gpBase->mpEffectHandler->GetPlayVoice()->SetVolumeMul(1.0f);

	mbActive = false;
}

//-----------------------------------------------------------------------


void cLuxEffect_PlayCommentary::Update(float afTimeStep)
{
	if(mpSoundHandler->IsValid(mpSoundEntry, mlSoundEntryID)) return;

	//Set the icon as not playing
	if(gpBase->mpMapHandler->GetCurrentMap())
	{
		iLuxEntity *pEntity = gpBase->mpMapHandler->GetCurrentMap()->GetEntityByID(mlIconID, eLuxEntityType_CommentaryIcon);
		if(pEntity)
		{
			cLuxCommentaryIcon *mpIcon = static_cast<cLuxCommentaryIcon*>(pEntity);
			mpIcon->SetPlayingSound(false);
		}
	}

	mpSoundEntry = NULL;

	mpSoundHandler->FadeGlobalVolume(1.0f,0.5f,  eSoundEntryType_World, eLuxGlobalVolumeType_Commentary, false);
	mpMusicHandler->FadeVolumeMul(1.0f, 0.5f);
	gpBase->mpEffectHandler->GetPlayVoice()->SetVolumeMul(1.0f);
	
	mbActive = false;
}

//-----------------------------------------------------------------------

void cLuxEffect_PlayCommentary::OnDraw(float afFrameTime)
{
}
//-----------------------------------------------------------------------

void cLuxEffect_PlayCommentary::Reset()
{
	if(mpSoundEntry != NULL && mpSoundHandler->IsValid(mpSoundEntry,mlSoundEntryID)) mpSoundEntry->Stop();
	mpSoundEntry = NULL;

	mpSoundHandler->SetGlobalVolume(1.0f,eSoundEntryType_World, eLuxGlobalVolumeType_Commentary);
	mpMusicHandler->SetVolumeMul(1.0f);
	
	mlIconID = -1;
	msTalker = "";
	msTopic = "";
}


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// EMOTION FLASH
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_EmotionFlash::cLuxEffect_EmotionFlash()
{
	mpWhiteGfx = gpBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1,1), eGuiMaterial_Additive);

	mpFont = NULL;

	mvFontSize = 20;

	mpActingPlayer = NULL;
}
cLuxEffect_EmotionFlash::~cLuxEffect_EmotionFlash()
{

}

//-----------------------------------------------------------------------

void cLuxEffect_EmotionFlash::ClearFonts()
{
	if(mpFont)
		gpBase->mpEngine->GetResources()->GetFontManager()->Destroy(mpFont);

	mpFont = NULL;
}

void cLuxEffect_EmotionFlash::LoadFonts()
{
	tString sFontFile = "game_default.fnt";
	mpFont = gpBase->mpEngine->GetResources()->GetFontManager()->CreateFontData(sFontFile);
}

//-----------------------------------------------------------------------

void cLuxEffect_EmotionFlash::Start(const tString &asTextCat, const tString &asTextEntry, const tString &asSound)
{
	mbActive = true;

	mlStep = 0;

	mfAlpha =0;

	gpBase->mpHelpFuncs->PlayGuiSoundData(asSound, eSoundEntryType_Gui);

	tWString sText = kTranslate(asTextCat, asTextEntry);
	mvTextRows.clear();
	mpFont->GetWordWrapRows(500, mvFontSize.y, mvFontSize, sText, &mvTextRows);

	mfTextTime = 3.0f + 0.15f * (float)sText.length();
	mfTextAlpha =0;

	//Coop: this vision belongs to the player who set it off. Remember them --
	//Start() runs inside their callback, so gpBase->mpPlayer is them right now,
	//but Update() and OnDraw() run from the module loop where it is always P1.
	//
	//This used to zoom and freeze BOTH players, which was the wrong reading of
	//"shared story beat": OnDraw only ever drew to mpGameHudSet, which is on P1's
	//viewport alone. So a P2-triggered flash zoomed and froze P1 while showing
	//them nothing, and the reverse for P2. Restoring still reaches both further
	//down -- that is what stops a FOV sticking when one player starts a sequence
	//and the other ends it.
	mpActingPlayer = gpBase->mpPlayer;

	gpBase->mpEffectHandler->SetPlayerIsPausedFor(mpActingPlayer, true);
	mpActingPlayer->FadeFOVMulTo(0.5f, 0.5f);

	//The blur belongs to the player who touched the stone, the same way the zoom
	//above does. There is one shared radial blur and cLuxMapHandler mirrors it
	//onto both halves, so without an owner Player 1 got tunnel vision from a
	//vision they were not in and could not see.
	gpBase->mpEffectHandler->SetRadialBlurOwner(mpActingPlayer);
	gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(0.15f, 3);
	gpBase->mpEffectHandler->GetRadialBlur()->SetBlurStartDist(0.6f);

	//Disable enemies
	gpBase->mpMapHandler->GetCurrentMap()->BroadcastEnemyMessage(eLuxEnemyMessage_Reset, false,0,0);

}

void cLuxEffect_EmotionFlash::Reset()
{
	mpActingPlayer = NULL;

	if(gpBase->mpEffectHandler) gpBase->mpEffectHandler->SetRadialBlurOwner(NULL);
}

//-----------------------------------------------------------------------

cLuxPlayer* cLuxEffect_EmotionFlash::GetActingPlayer()
{
	//Compared, never trusted -- P2 can be destroyed while a vision is running.
	if(mpActingPlayer && (mpActingPlayer == gpBase->mpPlayer || mpActingPlayer == gpBase->mpPlayer2))
		return mpActingPlayer;

	return gpBase->mpPlayer;
}

//-----------------------------------------------------------------------

void cLuxEffect_EmotionFlash::Update(float afTimeStep)
{
	if(mlStep ==0)
	{
		mfAlpha += 0.5f * afTimeStep;
		if(mfAlpha >= 1.0f)
		{
			mfAlpha = 1.0f;
			mlStep=1;
			mfCount = 1;
		}
	}
	else if(mlStep ==1)
	{
		mfTextAlpha += afTimeStep * 3.0f;
		if(mfTextAlpha > 1) mfTextAlpha =1;

		//Check if text has been displayed long enough.
		mfTextTime -= afTimeStep;
		if(mfTextTime < 0)
		{
			//Coop: the zoom went to one player, the restore goes to BOTH. A restore
			//cannot over-free anyone, and reaching both is what guarantees nobody is
			//left at 0.5 FOV by a vision the other player finished.
			gpBase->mpPlayer->FadeFOVMulTo(1.0f, 0.33f);
			{
				cLuxPlayer *pOtherFov = CoopGetOtherPlayerForEffects();
				if(pOtherFov) pOtherFov->FadeFOVMulTo(1.0f, 0.33f);
			}
			gpBase->mpEffectHandler->SetPlayerIsPaused(false);

			gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(0, 1);
			
			mlStep = 2;
		}
	}
	else if(mlStep ==2)
	{
		mfTextAlpha -= afTimeStep * 1.0f;
		if(mfTextAlpha < 0) mfTextAlpha =0;

		mfAlpha -= 0.33f * afTimeStep;
		if(mfAlpha <= 0.0f)
		{
			mbActive = false;

			//Released only now, not when the fade started: the blur is still fading
			//out over these last seconds and it is still this player's.
			if(gpBase->mpEffectHandler->GetRadialBlurOwner() == GetActingPlayer())
				gpBase->mpEffectHandler->SetRadialBlurOwner(NULL);
		}
	}
}

//-----------------------------------------------------------------------

void cLuxEffect_EmotionFlash::OnDraw(float afFrameTime)
{
	//Coop: mpGameHudSet is attached to P1's viewport only, so drawing there is
	//what made a P2-triggered vision appear on P1's half.
	cGuiSet *pSet = gpBase->mpGameHudSet;

	if(GetActingPlayer() == gpBase->mpPlayer2 && gpBase->mpMapHandler->GetCoopMode())
	{
		cGuiSet *pCoopSet = gpBase->mpMapHandler->GetCoopHudSet();
		if(pCoopSet) pSet = pCoopSet;
	}

	DrawVision(pSet);
}

//-----------------------------------------------------------------------

void cLuxEffect_EmotionFlash::DrawVision(cGuiSet *apSet)
{
	apSet->DrawGfx(mpWhiteGfx,gpBase->mvHudVirtualStartPos + cVector3f(0,0,3.2f),gpBase->mvHudVirtualSize,cColor(mfAlpha, 1));

	if(mfTextAlpha > 0)
	{
		float fStartY = 300 - (mvFontSize.y+2.0f) * 0.5f * (float)mvTextRows.size();

		if(mvTextRows.size() == 1)
		{
			apSet->DrawFont(mvTextRows[0], mpFont, cVector3f(400,fStartY, 4), mvFontSize, cColor(0, mfTextAlpha), eFontAlign_Center);
		}
		else
		{
			float fY = fStartY;
			for(size_t i=0; i<mvTextRows.size(); ++i)
			{
				apSet->DrawFont(mvTextRows[i], mpFont, cVector3f(150,fY, 4), mvFontSize, cColor(0, mfTextAlpha), eFontAlign_Left);
				fY += mvFontSize.y + 2.0f;
			}
		}
	}
}

//-----------------------------------------------------------------------

void cLuxEffect_EmotionFlash::DoAction(eLuxPlayerAction aAction, bool abPressed)
{
	if(abPressed==false) return;

	if(mlStep==1) mfTextTime = 0;
}

//-----------------------------------------------------------------------

void cLuxEffect_EmotionFlash::DoActionForPlayer(cLuxPlayer *apPlayer, eLuxPlayerAction aAction, bool abPressed)
{
	//The vision belongs to whoever set it off. It froze them and it is drawn on
	//their half alone, so it is theirs to press through -- and the other player,
	//who is still walking around and cannot see a word of it, must not be able to
	//skip text off their partner's screen with an ordinary interact click.
	//
	//GetActingPlayer falls back to Player 1 whenever the remembered actor is not
	//one of the live players, so a vision restored from a save, or one whose owner
	//has been destroyed, is still skippable. Nothing here can strand anybody in
	//any case: Update() ends the vision on its own timer whether or not a button
	//is ever pressed.
	if(apPlayer && apPlayer != GetActingPlayer()) return;

	DoAction(aAction, abPressed);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// RADIAL BLUR
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_RadialBlur::cLuxEffect_RadialBlur()
{
	mfSize =0;
	mfSizeGoal =0;
	mfBlurStartDist =0;
}

//-----------------------------------------------------------------------

void cLuxEffect_RadialBlur::SetBlurStartDist(float afDist)
{
	mfBlurStartDist = afDist;

	cPostEffectParams_RadialBlur radialBlurParams;
	radialBlurParams.mfSize = mfSize;
	radialBlurParams.mfBlurStartDist = mfBlurStartDist;
	gpBase->mpMapHandler->GetPostEffect_RadialBlur()->SetParams(&radialBlurParams);

}

//-----------------------------------------------------------------------

void cLuxEffect_RadialBlur::FadeTo(float afSize, float afSpeed)
{
	mfSizeGoal = afSize;
	mfFadeSpeed = afSpeed;
	SetActive(true);
	gpBase->mpMapHandler->GetPostEffect_RadialBlur()->SetActive(true);
}

//-----------------------------------------------------------------------

void cLuxEffect_RadialBlur::Update(float afTimeStep)
{
	if(mfSizeGoal < mfSize)
	{
		mfSize -= mfFadeSpeed * afTimeStep;
		if(mfSize <= mfSizeGoal)
		{
			mfSize = 	mfSizeGoal;
			SetActive(false);
		}
	}
	else
	{
		mfSize += mfFadeSpeed * afTimeStep;
		if(mfSize >= mfSizeGoal)
		{
			mfSize = mfSizeGoal;
			SetActive(false);
		}
	}

	cPostEffectParams_RadialBlur radialBlurParams;
	radialBlurParams.mfSize = mfSize;
	radialBlurParams.mfBlurStartDist = mfBlurStartDist;
	gpBase->mpMapHandler->GetPostEffect_RadialBlur()->SetParams(&radialBlurParams);

	if(mfSize <=0)
	{
		gpBase->mpMapHandler->GetPostEffect_RadialBlur()->SetActive(false);

		//The blur is over, so whoever it belonged to has stopped owning it. Done
		//HERE rather than by each source: there is one place a blur can end and
		//several that can start one, and an owner left behind would hold the next
		//blur -- a script's, meant for both halves -- on one screen.
		if(gpBase->mpEffectHandler) gpBase->mpEffectHandler->SetRadialBlurOwner(NULL);
	}
}

//-----------------------------------------------------------------------

void cLuxEffect_RadialBlur::Reset()
{
	mfSize =0;
	mfSizeGoal =0;
	mfBlurStartDist =0;

	gpBase->mpMapHandler->GetPostEffect_RadialBlur()->Reset();
	gpBase->mpMapHandler->GetPostEffect_RadialBlur()->SetActive(false);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SEPIA COLOR
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_SepiaColor::cLuxEffect_SepiaColor()
{
	// Alpha-blended fill, same construction as cLuxEffect_Flash's mpWhiteGfx.
	mpSepiaGfx = gpBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1,1), eGuiMaterial_Modulative);

	mfAmount =0;
	mfAmountGoal =0;
}

//-----------------------------------------------------------------------

void cLuxEffect_SepiaColor::DrawCoopOverlay(cGuiSet *apSet)
{
	if(apSet==NULL) return;
	if(mpSepiaGfx==NULL) return;

	float fAmount = cMath::Clamp(mfAmount, 0.0f, 1.0f);
	if(fAmount <= 0.0f) return;

	// MODULATIVE, not alpha. The quad multiplies the frame instead of painting over
	// it, so black stays black. The first attempt used an alpha overlay, which
	// lifted every dark pixel towards brown -- that is what made it read as an
	// orange filter smeared over the picture rather than a sepia grade.
	//
	// Each channel lerps from 1.0 (untouched) toward a warm multiplier, so the
	// effect fades in and out with the same 0..1 amount the real post effect uses
	// and is a complete no-op at 0. Red is left at 1.0; pulling green and blue down
	// is what produces the warm tone, and it darkens far less than a wash.
	cColor col(	1.0f,
				1.0f + (0.82f - 1.0f) * fAmount,
				1.0f + (0.62f - 1.0f) * fAmount,
				1.0f);

	apSet->DrawGfx(	mpSepiaGfx,
					gpBase->mvHudVirtualStartPos + cVector3f(0,0,3.1f),
					gpBase->mvHudVirtualSize,
					col, eGuiMaterial_Modulative);
}

void cLuxEffect_SepiaColor::FadeTo(float afAmount, float afSpeed)
{
	mfAmountGoal = afAmount;
	mfFadeSpeed = afSpeed;
	SetActive(true);
	gpBase->mpMapHandler->GetPostEffect_Sepia()->SetActive(true);
}

void cLuxEffect_SepiaColor::Update(float afTimeStep)
{
	if(mfAmountGoal < mfAmount)
	{
		mfAmount -= mfFadeSpeed * afTimeStep;
		if(mfAmount <= mfAmountGoal)
		{
			mfAmount = 	mfAmountGoal;
			SetActive(false);
		}
	}
	else
	{
		mfAmount += mfFadeSpeed * afTimeStep;
		if(mfAmount >= mfAmountGoal)
		{
			mfAmount = mfAmountGoal;
			SetActive(false);
		}
	}

	cPostEffectParams_ColorConvTex sepiaParams;
	sepiaParams.mfFadeAlpha = mfAmount;
	gpBase->mpMapHandler->GetPostEffect_Sepia()->SetParams(&sepiaParams);
	
	if(mfAmount <=0)
	{
		gpBase->mpMapHandler->GetPostEffect_Sepia()->SetActive(false);
	}
}

void cLuxEffect_SepiaColor::Reset()
{
	mfAmount =0;
	mfAmountGoal =0;
	gpBase->mpMapHandler->GetPostEffect_Sepia()->Reset();
	gpBase->mpMapHandler->GetPostEffect_Sepia()->SetActive(false);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SCREEN SHAKE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_ShakeScreen::cLuxEffect_ShakeScreen()
{
}

cLuxEffect_ShakeScreen::~cLuxEffect_ShakeScreen()
{
}

//-----------------------------------------------------------------------

void cLuxEffect_ShakeScreen::Start(float afAmount, float afTime,float afFadeInTime,float afFadeOutTime,
								   int alCoopTargetPlayer)
{
	cLuxEffect_ShakeScreen_Shake shake;
	shake.mlCoopTargetPlayer = alCoopTargetPlayer;
	shake.mfSize = afAmount;
	shake.mfMaxSize = afAmount;
	shake.mfTime = afTime;
	shake.mfFadeInTime = afFadeInTime;
	shake.mfMaxFadeInTime = afFadeInTime;
	shake.mfFadeOutTime = afFadeOutTime;
	shake.mfMaxFadeOutTime = afFadeOutTime;

	mlstShakes.push_back(shake);

	SetActive(true);
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//Coop: push the shake onto one specific player. Update() runs from the module
//loop, never inside an acting-player swap, so index 0 really is P1 here.
static void SetScreenShakeOnPlayer(int alPlayerIndex, const cVector3f &avAdd)
{
	cLuxPlayer *pPlayer = (alPlayerIndex == 1) ? gpBase->mpPlayer2 : gpBase->mpPlayer;
	if(pPlayer == NULL) return;

	pPlayer->SetHeadPosAdd(eLuxHeadPosAdd_ScreenShake, avAdd);
}

//-----------------------------------------------------------------------

void cLuxEffect_ShakeScreen::Update(float afTimeStep)
{
	//Per player, because a shake can now be aimed at one of them. An untargeted
	//shake counts towards both -- which is every shake a story that does not know
	//about co-op will ever produce, so those are unaffected.
	float fLargest[2] = { 0.0f, 0.0f };

	std::list<cLuxEffect_ShakeScreen_Shake>::iterator it = mlstShakes.begin();
	for(; it != mlstShakes.end(); )
	{
		cLuxEffect_ShakeScreen_Shake &shake = *it;

		if(shake.mfFadeInTime >0)
		{
			shake.mfFadeInTime -= afTimeStep; if(shake.mfFadeInTime<0) shake.mfFadeInTime=0;
			float fT = shake.mfFadeInTime / shake.mfMaxFadeInTime;
			shake.mfSize = (1-fT) * shake.mfMaxSize;
		}
		else if(shake.mfTime >0)
		{
			shake.mfTime -= afTimeStep; if(shake.mfTime<0)shake.mfTime=0;
			shake.mfSize = shake.mfMaxSize;
		}
		else
		{
			shake.mfFadeOutTime -= afTimeStep; if(shake.mfFadeOutTime<0) shake.mfFadeOutTime=0;
			float fT = shake.mfFadeOutTime / shake.mfMaxFadeOutTime;
			shake.mfSize =  fT * shake.mfMaxSize;
		}

		//Log("%f, %f, %f size: %f\n",shake.mfFadeInTime,shake.mfTime,shake.mfFadeOutTime,shake.mfSize);

		for(int i=0; i<2; ++i)
			if(shake.mlCoopTargetPlayer < 0 || shake.mlCoopTargetPlayer == i)
				if(fLargest[i] < shake.mfSize) fLargest[i] = shake.mfSize;

		if(shake.mfTime <= 0 && shake.mfFadeOutTime <= 0 && shake.mfFadeInTime <= 0)
		{
			it = mlstShakes.erase(it);

			//If all shaking is over, set pos add to 0 and return.
			if(mlstShakes.empty())
			{
				SetActive(false);
				SetScreenShakeOnPlayer(0, 0);
				SetScreenShakeOnPlayer(1, 0);
				return;
			}
		}
		else
		{
			++it;
		}
	}

	//A shake with no target is a physical event in the MAP -- a barrel landing, a
	//collapse, an explosion -- so both players feel it whoever set it off. Each
	//camera gets its own random draw at its own magnitude, so the two views jitter
	//naturally instead of in lockstep.
	//
	//A player whose largest has fallen to zero gets a zero written out here, which
	//is what puts them back straight when a shake aimed only at them ends while
	//somebody else's is still running.
	for(int i=0; i<2; ++i)
	{
		cVector3f vAdd(0);
		vAdd.x = cMath::RandRectf(-fLargest[i],fLargest[i]);
		vAdd.y = cMath::RandRectf(-fLargest[i],fLargest[i]);
		vAdd.z = cMath::RandRectf(-fLargest[i],fLargest[i]);

		SetScreenShakeOnPlayer(i, vAdd);
	}
}

//-----------------------------------------------------------------------

void cLuxEffect_ShakeScreen::Reset()
{
	mlstShakes.clear();
}


//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// IMAGE TRAIL
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_ImageTrail::cLuxEffect_ImageTrail()
{
	mfAmount =0;
	mfAmountGoal =0;
}

void cLuxEffect_ImageTrail::FadeTo(float afAmount, float afSpeed)
{
	mfAmountGoal = afAmount;
	mfFadeSpeed = afSpeed;
	SetActive(true);
	gpBase->mpMapHandler->GetPostEffect_ImageTrail()->SetActive(true);
}

void cLuxEffect_ImageTrail::Update(float afTimeStep)
{
	if(mfAmountGoal < mfAmount)
	{
		mfAmount -= mfFadeSpeed * afTimeStep;
		if(mfAmount <= mfAmountGoal)
		{
			mfAmount = 	mfAmountGoal;
			SetActive(false);
		}
	}
	else
	{
		mfAmount += mfFadeSpeed * afTimeStep;
		if(mfAmount >= mfAmountGoal)
		{
			mfAmount = mfAmountGoal;
			SetActive(false);
		}
	}
	
	cPostEffectParams_ImageTrail imageTrailParams;
	imageTrailParams.mfAmount = mfAmount;
	gpBase->mpMapHandler->GetPostEffect_ImageTrail()->SetParams(&imageTrailParams);
	
	if(mfAmount <=0)
	{
		gpBase->mpMapHandler->GetPostEffect_ImageTrail()->SetActive(false);
	}
}

void cLuxEffect_ImageTrail::Reset()
{
	mfAmount =0;
	mfAmountGoal =0;
	gpBase->mpMapHandler->GetPostEffect_ImageTrail()->Reset();
	gpBase->mpMapHandler->GetPostEffect_ImageTrail()->SetActive(false);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// FADE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_Fade::cLuxEffect_Fade()
{
	mpWhiteGfx = gpBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1,1), eGuiMaterial_Modulative);

	Reset();
}

cLuxEffect_Fade::~cLuxEffect_Fade()
{

}

//-----------------------------------------------------------------------

void cLuxEffect_Fade::SetCoopTarget(bool abGlobal)
{
	if(abGlobal || gpBase->mpPlayer == NULL)
	{
		mbShowOnP1 = true;
		mbShowOnP2 = true;
		return;
	}

	//gpBase->mpPlayer is the acting player: interact, use-item, look-at,
	//trigger volumes and timers all point it at whoever set the scene off.
	bool bP2 = gpBase->mpPlayer->IsPlayer2();
	mbShowOnP1 = !bP2;
	mbShowOnP2 = bP2;
}

void cLuxEffect_Fade::FadeIn(float afTime)
{
	SetCoopTarget(false);
	ApplyFade(0, afTime);
}

void cLuxEffect_Fade::FadeOut(float afTime)
{
	SetCoopTarget(false);
	ApplyFade(1, afTime);
}

//-----------------------------------------------------------------------

void cLuxEffect_Fade::ApplyFade(float afGoal, float afTime)
{
	//Only the halves this fade was aimed at. The other half keeps doing whatever it
	//was doing -- that independence is the whole point of splitting the state.
	for(int i=0; i<2; ++i)
	{
		if(i==0 && mbShowOnP1==false) continue;
		if(i==1 && mbShowOnP2==false) continue;

		mfGoalAlpha[i] = afGoal;

		if(afTime <= 0)	mfAlpha[i] = afGoal;
		else			mfFadeSpeed[i] = 1 / afTime;
	}

	SetActive(true);
}

//-----------------------------------------------------------------------

void cLuxEffect_Fade::SetDirectAlpha(float afX)
{
	//Global, always. Its only caller is the ManPig mind-fuck blink, which has no
	//player context -- so it used to inherit whichever half the last scripted fade
	//happened to target, landing a full-screen black blink on an arbitrary player
	//and able to leave them sitting at alpha 1. Both halves blink, or neither.
	SetCoopTarget(true);

	if(afX<=0)	SetActive(false);
	else		SetActive(true);

	mfGoalAlpha[0] = mfGoalAlpha[1] = afX;
	mfAlpha[0] = mfAlpha[1] = afX;
}

//-----------------------------------------------------------------------

bool cLuxEffect_Fade::IsFading()
{
	for(int i=0; i<2; ++i)
	{
		if(mfGoalAlpha[i]==0 && mfAlpha[i]>0) return true;
		if(mfGoalAlpha[i]==1 && mfAlpha[i]<1) return true;
	}

	return false;
}

//-----------------------------------------------------------------------

void cLuxEffect_Fade::Update(float afTimeStep)
{
	////////////////////////////////////////////////////////////////////////
	// Coop hygiene: while coop is off, index 1 has no set to draw to (OnDraw
	// skips it), but a non-zero mfAlpha[1] left over from a session -- or from
	// SetDirectAlpha, which always writes both -- kept the effect permanently
	// Active here, and repainted P2's half instantly black the moment a coop
	// hud set existed again. Snap it clear while it cannot be seen.
	if(gpBase->mpMapHandler->GetCoopMode()==false)
	{
		mfAlpha[1] = 0;
		mfGoalAlpha[1] = 0;
	}

	bool bAnythingStillCovered = false;

	for(int i=0; i<2; ++i)
	{
		////////////////////////////////////////////////////////////////////
		// Converge toward the goal, WHATEVER it is. The old tests compared the
		// goal against exactly 0 and exactly 1 -- but SetDirectAlpha (the ManPig
		// blink) parks fractional goals, and any such value froze that half's
		// alpha forever: a permanently dimmed screen with the effect still
		// Active, immune to every fade that only wrote the *other* index.
		if(mfAlpha[i] > mfGoalAlpha[i])
		{
			mfAlpha[i] -= afTimeStep * mfFadeSpeed[i];
			if(mfAlpha[i] < mfGoalAlpha[i]) mfAlpha[i] = mfGoalAlpha[i];
		}
		else if(mfAlpha[i] < mfGoalAlpha[i])
		{
			mfAlpha[i] += afTimeStep * mfFadeSpeed[i];
			if(mfAlpha[i] > mfGoalAlpha[i]) mfAlpha[i] = mfGoalAlpha[i];
		}

		if(mfAlpha[i] > 0) bAnythingStillCovered = true;
	}

	//Only once BOTH halves are clear. Deactivating while one was still black stopped
	//the only thing that could have lifted it.
	if(bAnythingStillCovered==false) SetActive(false);
}

//-----------------------------------------------------------------------

void cLuxEffect_Fade::OnDraw(float afFrameTime)
{
	//Answers one question and nothing else: was this effect covering a half at the
	//moment the screen went black? Silent unless a half is at least 70% covered, and
	//only on a change, so a normal level-transition fade logs twice and idle play
	//logs nothing at all. If the screen blacks out with no line here, this is not it.
	{
		static int slPrevState = -1;
		const int lState = (mfAlpha[0] > 0.7f ? 1 : 0) | (mfAlpha[1] > 0.7f ? 2 : 0);

		if(lState != slPrevState)
		{
			slPrevState = lState;
			if(lState != 0)
				LogVerbose("[fade covering] P1=%.2f P2=%.2f at %lu ms\n",
					mfAlpha[0], mfAlpha[1], cPlatform::GetApplicationTime());
		}
	}

	if(mfAlpha[0] > 0)
	{
		gpBase->mpGameHudSet->DrawGfx(mpWhiteGfx,gpBase->mvHudVirtualStartPos+cVector3f(0,0,3.2f),gpBase->mvHudVirtualSize,cColor(1-mfAlpha[0], 1));
	}

	//mpGameHudSet is only attached to P1's viewport in split-screen, so P2 needs its
	//own draw, from its own alpha.
	if(mfAlpha[1] > 0 && gpBase->mpMapHandler->GetCoopMode())
	{
		cGuiSet *pCoopSet = gpBase->mpMapHandler->GetCoopHudSet();
		if(pCoopSet)
			pCoopSet->DrawGfx(mpWhiteGfx,gpBase->mvHudVirtualStartPos+cVector3f(0,0,3.2f),gpBase->mvHudVirtualSize,cColor(1-mfAlpha[1], 1));
	}
}

//-----------------------------------------------------------------------

void cLuxEffect_Fade::FadeInGlobal(float afTime)
{
	//Target first, THEN apply. The old order called FadeIn -- which re-targeted to a
	//single player -- and only widened the flags afterwards, so the second half never
	//got a goal or a speed and simply held whatever it already had.
	SetCoopTarget(true);
	ApplyFade(0, afTime);
}
void cLuxEffect_Fade::FadeOutGlobal(float afTime)
{
	SetCoopTarget(true);
	ApplyFade(1, afTime);
}
void cLuxEffect_Fade::Reset()
{
	mbShowOnP1 = true;
	mbShowOnP2 = true;

	for(int i=0; i<2; ++i)
	{
		mfGoalAlpha[i] = 0;
		mfAlpha[i] = 0;
		mfFadeSpeed[i] = 1;
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SANITY GAIN FLASH
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_SanityGainFlash::cLuxEffect_SanityGainFlash()
{
	mpWhiteGfx = gpBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1,1), eGuiMaterial_Additive);

	mColor = gpBase->mpGameCfg->GetColor("Player_General","SanityGain_Color", 0);
	msSound = gpBase->mpGameCfg->GetString("Player_General","SanityGain_Sound", "");
	mfFadeInTime = gpBase->mpGameCfg->GetFloat("Player_General","SanityGain_FadeInTime", 0);
	mfFadeOutTime = gpBase->mpGameCfg->GetFloat("Player_General","SanityGain_FadeOutTime", 0);

	Reset();
}
cLuxEffect_SanityGainFlash::~cLuxEffect_SanityGainFlash()
{

}

//-----------------------------------------------------------------------

void cLuxEffect_SanityGainFlash::Reset()
{
	mfAlpha =0;

	mbShowOnP1 = false;
	mbShowOnP2 = false;
}

//-----------------------------------------------------------------------

void cLuxEffect_SanityGainFlash::Start()
{
	// No player context, so both screens. StartForPlayer() narrows it after.
	mbShowOnP1 = true;
	mbShowOnP2 = true;

	if(msSound != "")
		gpBase->mpHelpFuncs->PlayGuiSoundData(msSound, eSoundEntryType_Gui);
	
	mbActive = true;

	mlStep = 0;

	mfAlpha =0;

	mfFadeInSpeed = 1 / mfFadeInTime;
	mfWhiteSpeed = 1 / 0.05f;
	mfFadeOutSpeed = 1 / mfFadeOutTime;
}

//-----------------------------------------------------------------------

void cLuxEffect_SanityGainFlash::StartForPlayer(cLuxPlayer *apPlayer)
{
	bool bP2 = (apPlayer != NULL && apPlayer->IsPlayer2());
	bool bAlreadyShowing = bP2 ? mbShowOnP2 : mbShowOnP1;

	//Already flashing on this player's screen: leave it alone rather than
	//restarting the timeline and playing the sound twice.
	if(mbActive && bAlreadyShowing) return;

	if(mbActive)
	{
		//The other player's flash is mid-run -- join it instead of restarting,
		//so neither flash is cut short. This is the path a shared reward takes:
		//both players boosted in the same frame, so one flash and one sound,
		//shown on both halves of the screen.
		if(bP2)	mbShowOnP2 = true;
		else	mbShowOnP1 = true;
		return;
	}

	Start();

	mbShowOnP1 = !bP2;
	mbShowOnP2 = bP2;
}

//-----------------------------------------------------------------------

void cLuxEffect_SanityGainFlash::Update(float afTimeStep)
{
	if(mlStep ==0)
	{
		mfAlpha += mfFadeInSpeed * afTimeStep;
		if(mfAlpha >= 1.0f)
		{
			mfAlpha = 1.0f;
			mlStep=1;
			mfCount = 1;
		}
	}
	else if(mlStep ==1)
	{
		mfCount -= mfWhiteSpeed * afTimeStep;
		if(mfCount <= 0)
		{
			mlStep = 2;
		}
	}
	else if(mlStep ==2)
	{
		mfAlpha -= mfFadeOutSpeed * afTimeStep;
		if(mfAlpha <= 0.0f)
		{
			mbActive = false;
		}
	}

}

//-----------------------------------------------------------------------

void cLuxEffect_SanityGainFlash::OnDraw(float afFrameTime)
{
	if(mbShowOnP1) DrawFlash(gpBase->mpGameHudSet, afFrameTime);

	//Coop: mpGameHudSet is only attached to P1's viewport in split-screen, so
	//a reward P2 earned used to flash on P1's half and look like nothing had
	//happened at all on P2's.
	if(mbShowOnP2 && gpBase->mpMapHandler->GetCoopMode())
	{
		cGuiSet *pCoopSet = gpBase->mpMapHandler->GetCoopHudSet();
		if(pCoopSet) DrawFlash(pCoopSet, afFrameTime);
	}
}	

//-----------------------------------------------------------------------

void cLuxEffect_SanityGainFlash::DrawFlash(cGuiSet *apSet ,float afTimeStep)
{
	apSet->DrawGfx(mpWhiteGfx,gpBase->mvHudVirtualStartPos+cVector3f(0,0,3.2f),gpBase->mvHudVirtualSize,mColor*mfAlpha);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// FLASH
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_Flash::cLuxEffect_Flash()
{
	mpWhiteGfx = gpBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1,1), eGuiMaterial_Additive);
	Reset();
}
cLuxEffect_Flash::~cLuxEffect_Flash()
{
	
}

//-----------------------------------------------------------------------

void cLuxEffect_Flash::StartTimeline(float afFadeIn, float afWhite, float afFadeOut)
{
	mbActive = true;

	mlStep = 0;

	mfAlpha =0;

	if(afFadeIn==0) afFadeIn = 0.000001f;
	if(afWhite==0) afWhite = 0.000001f;
	if(afFadeOut==0) afFadeOut = 0.000001f;


	mfFadeInSpeed = 1 / afFadeIn;
	mfWhiteSpeed = 1 / afWhite;
	mfFadeOutSpeed = 1 / afFadeOut;
}

//-----------------------------------------------------------------------

void cLuxEffect_Flash::Start(float afFadeIn, float afWhite, float afFadeOut)
{
	// No player context, so both halves -- this is the scripted / flashback
	// path and those are shared story beats. StartForPlayer narrows it.
	mbShowOnP1 = true;
	mbShowOnP2 = true;

	StartTimeline(afFadeIn, afWhite, afFadeOut);
}

//-----------------------------------------------------------------------

void cLuxEffect_Flash::StartForPlayer(cLuxPlayer *apPlayer, float afFadeIn, float afWhite, float afFadeOut)
{
	bool bP2 = (apPlayer != NULL && apPlayer->IsPlayer2());
	bool bAlreadyShowing = bP2 ? mbShowOnP2 : mbShowOnP1;

	//Already flashing on this half: leave the timeline alone rather than
	//restarting it and re-whitening a flash that is already fading out.
	if(mbActive && bAlreadyShowing) return;

	//The other half is mid-flash -- join it instead of restarting, so neither
	//is cut short. Same reasoning as cLuxEffect_SanityGainFlash::StartForPlayer.
	if(mbActive)
	{
		if(bP2)	mbShowOnP2 = true;
		else	mbShowOnP1 = true;
		return;
	}

	mbShowOnP1 = !bP2;
	mbShowOnP2 =  bP2;

	StartTimeline(afFadeIn, afWhite, afFadeOut);
}

//-----------------------------------------------------------------------

void cLuxEffect_Flash::Reset()
{
	mlStep = 0;

	mfAlpha =0;

	//Both, deliberately. Every path that does not name an owner is a shared
	//flash, and a stale one-sided state must never be able to swallow one.
	mbShowOnP1 = true;
	mbShowOnP2 = true;
}

//-----------------------------------------------------------------------

void cLuxEffect_Flash::Update(float afTimeStep)
{
	if(mlStep ==0)
	{
		mfAlpha += mfFadeInSpeed * afTimeStep;
		if(mfAlpha >= 1.0f)
		{
			mfAlpha = 1.0f;
			mlStep=1;
			mfCount = 1;
		}
	}
	else if(mlStep ==1)
	{
		mfCount -= mfWhiteSpeed * afTimeStep;
		if(mfCount <= 0)
		{
			mlStep = 2;
		}
	}
	else if(mlStep ==2)
	{
		mfAlpha -= mfFadeOutSpeed * afTimeStep;
		if(mfAlpha <= 0.0f)
		{
			mbActive = false;
		}
	}

}

//-----------------------------------------------------------------------

void cLuxEffect_Flash::OnDraw(float afFrameTime)
{
	if(mbShowOnP1)
		gpBase->mpGameHudSet->DrawGfx(mpWhiteGfx,gpBase->mvHudVirtualStartPos+cVector3f(0,0,3.2f),gpBase->mvHudVirtualSize,cColor(mfAlpha, 1));

	//Coop: mpGameHudSet is only attached to P1's viewport in split-screen, so mirror
	//the flash onto P2's hud set. Without this P2 sees no flashback flash at all.
	//
	//Gated now. This mirror used to be unconditional, which also dragged the
	//RESPAWN flash onto P2 -- and the rest of the death sequence (fade, hint) is
	//drawn only on the half running it, so P2 got a bare white flash with nothing
	//leading up to it. Scripted and flashback flashes still reach both halves;
	//they go through Start(), which sets both flags.
	if(mbShowOnP2 && gpBase->mpMapHandler->GetCoopMode())
	{
		cGuiSet *pCoopSet = gpBase->mpMapHandler->GetCoopHudSet();
		if(pCoopSet)
			pCoopSet->DrawGfx(mpWhiteGfx,gpBase->mvHudVirtualStartPos+cVector3f(0,0,3.2f),gpBase->mvHudVirtualSize,cColor(mfAlpha, 1));
	}
}


//////////////////////////////////////////////////////////////////////////
// PLAY VOICE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxEffect_PlayVoice::cLuxEffect_PlayVoice()
{
	mpSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();

	mvFontSize = gpBase->mpGameCfg->GetVector2f("Effects","VoiceTextFontSize",1);
	mfRowWidth = gpBase->mpGameCfg->GetFloat("Effects","VoiceTextRowWidth",1);

	mpVoiceEntry = NULL;
	mlVoiceEntryID = -1;
	mpEffectEntry = NULL;
	mlEffectEntryID = -1;

	Reset();
}

cLuxEffect_PlayVoice::~cLuxEffect_PlayVoice()
{
	Reset();
}

//-----------------------------------------------------------------------

void cLuxEffect_PlayVoice::StopVoices(float afFadeOutSpeed)
{
	if(mpSoundHandler->IsValid(mpVoiceEntry,mlVoiceEntryID)) mpVoiceEntry->FadeOut(afFadeOutSpeed);
	mpVoiceEntry = NULL;

	if(mpSoundHandler->IsValid(mpEffectEntry,mlEffectEntryID)) mpEffectEntry->FadeOut(afFadeOutSpeed);
	mpEffectEntry = NULL;
	
	mlstVoices.clear();

	mbActive = false;
}
//-----------------------------------------------------------------------

void cLuxEffect_PlayVoice::AddVoice(const tString& asVoiceFile, const tString& asEffectFile,
									const tString& asTextCat, const tString& asTextEntry, bool abUsePostion, 
									const cVector3f& avPosition, float afMinDistance, float afMaxDistance)
{
	cLuxVoiceData voiceData;

	//Log("Adding sounds: '%s' and '%s'\n", asVoiceFile.c_str(), asEffectFile.c_str());
	
    voiceData.msVoiceFile = asVoiceFile;
	voiceData.msEffectFile = asEffectFile;
	if(asTextCat != "" && asTextEntry != "")
		voiceData.msText = kTranslate(asTextCat, asTextEntry);
	else
		voiceData.msText = _W("");
	voiceData.mbUsePosition = abUsePostion;
	voiceData.mvPosition = avPosition;
	voiceData.mfMinDistance = afMinDistance;
	voiceData.mfMaxDistance = afMaxDistance;

	mlstVoices.push_back(voiceData);

	mbActive = true;
}

//-----------------------------------------------------------------------

void cLuxEffect_PlayVoice::PauseCurrentVoices()
{
	if(mbActive==false || mbPaused) return;

	mbPaused = true;
	
	//Voice
	if(mpVoiceEntry && mpSoundHandler->IsValid(mpVoiceEntry, mlVoiceEntryID))
	{
		mpVoiceEntry->SetPaused(true);
	}

	//Effect
	if(mpEffectEntry && mpSoundHandler->IsValid(mpEffectEntry, mlEffectEntryID))
	{
		mpEffectEntry->SetPaused(true);
	}
}

//-----------------------------------------------------------------------

void cLuxEffect_PlayVoice::UnpauseCurrentVoices()
{
	if(mbActive==false || mbPaused==false) return;
	
	mbPaused = false;

	//Voice
	if(mpVoiceEntry && mpSoundHandler->IsValid(mpVoiceEntry, mlVoiceEntryID))
	{
		mpVoiceEntry->SetPaused(false);
	}

	//Effect
	if(mpEffectEntry && mpSoundHandler->IsValid(mpEffectEntry, mlEffectEntryID))
	{
		mpEffectEntry->SetPaused(false);
	}
}

//-----------------------------------------------------------------------


void cLuxEffect_PlayVoice::Update(float afTimeStep)
{
	//do not want to have like this, because then loading save when playing last voice + callback will not work and callback will not be called.
	//if(mpVoiceEntry==NULL && mpEffectEntry==NULL && mlstVoices.empty()) return; 

	if(mfVolumeMul <1.0f)
	{
		if(mpVoiceEntry && mpSoundHandler->IsValid(mpVoiceEntry, mlVoiceEntryID))
			mpVoiceEntry->SetVolumeMul(mfVolumeMul);

		if(mpEffectEntry && mpSoundHandler->IsValid(mpEffectEntry, mlEffectEntryID))
			mpEffectEntry->SetVolumeMul(mfVolumeMul);
	}
	
	if(mpSoundHandler->IsValid(mpVoiceEntry, mlVoiceEntryID)) return;
	if(mpVoiceEntry==NULL && mpSoundHandler->IsValid(mpEffectEntry, mlEffectEntryID)) return;

	if(mlstVoices.empty())
	{
		//Need to save as it will be reseted otherwise!
		tString sCallback = msOverCallback; 

		//Reset before calling so it is possible to start voices from callback!
		float fPreVolMul = mfVolumeMul;
		Reset();
		mfVolumeMul = fPreVolMul;
		SetActive(false);

		if(sCallback!="")
			gpBase->mpMapHandler->GetCurrentMap()->RunScript(sCallback+"()");
		
		return;
	}

    cLuxVoiceData& voiceData = mlstVoices.front();
	
	//////////////////////
	//GUI sound
	if(voiceData.mbUsePosition==false)
	{
		mpVoiceEntry = mpSoundHandler->PlayGuiStream(voiceData.msVoiceFile,false, 1.0f);
		if(mpVoiceEntry) mlVoiceEntryID = mpVoiceEntry->GetId();
		
		if(voiceData.msEffectFile!="")
		{
			mpEffectEntry = mpSoundHandler->PlayGuiStream(voiceData.msEffectFile,false, 1.0f);
			if(mpEffectEntry) mlEffectEntryID = mpEffectEntry->GetId();
		}
	}
	//////////////////////
	//3D sound with position
	else
	{
		mpVoiceEntry = mpSoundHandler->Play(voiceData.msVoiceFile,false, 1.0f, voiceData.mvPosition,voiceData.mfMinDistance, voiceData.mfMaxDistance,
											eSoundEntryType_Gui,false,true,0, true);
		if(mpVoiceEntry) mlVoiceEntryID = mpVoiceEntry->GetId();
		
		if(voiceData.msEffectFile!="")
		{
			mpEffectEntry = mpSoundHandler->Play(	voiceData.msEffectFile,false, 1.0f, voiceData.mvPosition,voiceData.mfMinDistance, voiceData.mfMaxDistance,
													eSoundEntryType_Gui,false,true,0, true);
			if(mpEffectEntry) mlEffectEntryID = mpEffectEntry->GetId();
		}
	}

	//////////////////////
	//Text
	mvCurrentTextRows.clear();
	if(voiceData.msText != _W(""))
		gpBase->mpDefaultFont->GetWordWrapRows(mfRowWidth,mvFontSize.y+2,mvFontSize, voiceData.msText, &mvCurrentTextRows);

	//////////////////////
	//Pop!
	mlstVoices.pop_front();

	//////////////////////
	//Extra check in case the voices does not load.
	if(mlstVoices.empty() && mpVoiceEntry==NULL && mpEffectEntry==NULL)
	{
		//Reset before calling so it is possible to start voices from callback!
		Reset();
		SetActive(false);
		
		if(msOverCallback!="")
			gpBase->mpMapHandler->GetCurrentMap()->RunScript(msOverCallback+"()");
	}
}

//-----------------------------------------------------------------------

void cLuxEffect_PlayVoice::OnDraw(float afFrameTime)
{
	if(gpBase->mpMessageHandler->ShowSubtitles()==false) return;
	if(mvCurrentTextRows.empty()) return;

	//Coop: mpGameHudSet is only attached to P1's viewport in split-screen, which is
	//why P2 saw no voice subtitles at all (flashback lines included). There is only
	//one global cLuxEffect_PlayVoice with one row list, so the same text is mirrored
	//onto P2's hud set rather than tracked per player.
	cGuiSet *pCoopSet = NULL;
	if(gpBase->mpMapHandler->GetCoopMode())
		pCoopSet = gpBase->mpMapHandler->GetCoopHudSet();

	cVector3f vStartPos(400-mfRowWidth/2, 580 - (mvCurrentTextRows.size()*(mvFontSize.y+2)), 4);

    for(size_t i=0; i<mvCurrentTextRows.size(); ++i)
	{
		gpBase->mpGameHudSet->DrawFont(mvCurrentTextRows[i],gpBase->mpDefaultFont, vStartPos, mvFontSize,cColor(1,1));

		if(pCoopSet)
			pCoopSet->DrawFont(mvCurrentTextRows[i],gpBase->mpDefaultFont, vStartPos, mvFontSize,cColor(1,1));

		vStartPos.y+= mvFontSize.y+2;
	}
}
//-----------------------------------------------------------------------

void cLuxEffect_PlayVoice::Reset()
{
	if(mpVoiceEntry != NULL && mpSoundHandler->IsValid(mpVoiceEntry,mlVoiceEntryID)) mpVoiceEntry->Stop();
	mpVoiceEntry = NULL;

	if(mpEffectEntry != NULL && mpSoundHandler->IsValid(mpEffectEntry,mlEffectEntryID)) mpEffectEntry->Stop();
	mpEffectEntry = NULL;
	
	mvCurrentTextRows.clear();

	mlstVoices.clear();
	
	msOverCallback = "";

	mbPaused = false;

	mfVolumeMul = 1.0f;
}

void cLuxEffect_PlayVoice::SetVolumeMul(float afMul)
{
	mfVolumeMul = afMul;

	if(mpVoiceEntry && mpSoundHandler->IsValid(mpVoiceEntry, mlVoiceEntryID))
		mpVoiceEntry->SetVolumeMul(mfVolumeMul);

	if(mpEffectEntry && mpSoundHandler->IsValid(mpEffectEntry, mlEffectEntryID))
		mpEffectEntry->SetVolumeMul(mfVolumeMul);    
}

//-----------------------------------------------------------------------

bool cLuxEffect_PlayVoice::VoiceDonePlaying()
{
	if(mlstVoices.empty() && mpVoiceEntry==NULL)
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxEffectHandler::OnClearFonts()
{
	mpEmotionFlash->ClearFonts();
}

void cLuxEffectHandler::LoadFonts()
{
	mpEmotionFlash->LoadFonts();
}

void cLuxEffectHandler::OnStart()
{
}

//-----------------------------------------------------------------------


void cLuxEffectHandler::Reset()
{
	/////////////////////////
	// Effects
	for(size_t i=0; i<mvEffects.size(); ++i)
	{
		iLuxEffect *pEffect = mvEffects[i];
		pEffect->Reset();
		pEffect->SetActive(false);
	}

	/////////////////////////
	// World sound mul
    for(int i=0; i<eLuxGlobalVolumeType_LastEnum; ++i)
	{
		gpBase->mpEngine->GetSound()->GetSoundHandler()->SetGlobalSpeed(1, eSoundEntryType_World, i);
		gpBase->mpEngine->GetSound()->GetSoundHandler()->SetGlobalVolume(1, eSoundEntryType_World, i);
	}

	mbPlayerIsPaused = false;
	mpRadialBlurOwner = NULL;
}

//-----------------------------------------------------------------------

void cLuxEffectHandler::Update(float afTimeStep)
{
	for(size_t i=0; i<mvEffects.size(); ++i)
	{
		iLuxEffect *pEffect = mvEffects[i];
		if(pEffect->IsActive()) pEffect->Update(afTimeStep);
	}
}

//-----------------------------------------------------------------------

void cLuxEffectHandler::OnMapEnter(cLuxMap *apMap)
{
	
}

//-----------------------------------------------------------------------

void cLuxEffectHandler::OnMapLeave(cLuxMap *apMap)
{
	//////////////////
	// Reset some effects on map leave
	mpSepiaColor->FadeTo(0, 1);
	mpRadialBlur->FadeTo(0, 1);
	if(mpPlayCommentary->IsActive()) mpPlayCommentary->Stop();
}

//-----------------------------------------------------------------------



void cLuxEffectHandler::OnDraw(float afFrameTime)
{
	for(size_t i=0; i<mvEffects.size(); ++i)
	{
		iLuxEffect *pEffect = mvEffects[i];
		if(pEffect->IsActive()) pEffect->OnDraw(afFrameTime);
	}
}

//-----------------------------------------------------------------------

void cLuxEffectHandler::DoAction(eLuxPlayerAction aAction, bool abPressed)
{
	for(size_t i=0; i<mvEffects.size(); ++i)
	{
		iLuxEffect *pEffect = mvEffects[i];
		if(pEffect->IsActive()) pEffect->DoAction(aAction, abPressed);
	}
}

//-----------------------------------------------------------------------

void cLuxEffectHandler::DoActionForPlayer(cLuxPlayer *apPlayer, eLuxPlayerAction aAction, bool abPressed)
{
	for(size_t i=0; i<mvEffects.size(); ++i)
	{
		iLuxEffect *pEffect = mvEffects[i];
		if(pEffect->IsActive()) pEffect->DoActionForPlayer(apPlayer, aAction, abPressed);
	}
}

//-----------------------------------------------------------------------

void cLuxEffectHandler::SetPlayerIsPaused(bool abX)
{
	SetPlayerIsPausedFor(gpBase->mpPlayer, abX);
}

void cLuxEffectHandler::SetPlayerIsPausedFor(cLuxPlayer *apPlayer, bool abX)
{
	mbPlayerIsPaused = abX;

	////////////////////////////////
	// Freezing is the triggering player's business.
	if(abX)
	{
		if(apPlayer) apPlayer->SetActive(false);
		return;
	}

	////////////////////////////////
	// Un-freezing is not. The pause is set from the triggering player's callback
	// but cleared from the module loop, where gpBase->mpPlayer is always P1, so
	// releasing only the actor would strand a P2 who triggered a vision. A
	// release can never over-free anyone, so it reaches both.
	gpBase->mpPlayer->SetActive(true);

	cLuxPlayer *pOther = CoopGetOtherPlayerForEffects();
	if(pOther) pOther->SetActive(true);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------


//-----------------------------------------------------------------------



