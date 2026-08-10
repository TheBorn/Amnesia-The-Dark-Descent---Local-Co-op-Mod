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

#include "LuxPlayerHelpers.h"

#include "LuxPlayer.h"
#include "LuxMapHelper.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxMoveState_Normal.h"
#include "LuxMusicHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxInventory.h"
#include "LuxEffectHandler.h"
#include "LuxEnemy.h"
//cLuxPlayerPossess calls GetMover()->MoveToPos, and LuxEnemy.h only forward
//declares cLuxEnemyMover.
#include "LuxEnemyMover.h"
#include "LuxProp.h"
#include "LuxProp_CritterBase.h"
#include "LuxHintHandler.h"
#include "LuxCompletionCountHandler.h"
#include "LuxInputHandler.h"
#include "LuxSaveHandler.h"
#include "LuxPlayerHands.h"
#include "LuxPostEffects.h"
#include "LuxProgressLogHandler.h"
#include "LuxDebugHandler.h"
#include "LuxPlayerState.h"
#include "LuxLoadScreenHandler.h"
#include "LuxMainMenu.h"
#include "impl/ImGuiDebugMenu.h"

//Debug gun. Mesh/material/texture types come in through LuxBase.h the same way
//cLuxPlayerAvatar gets them; only the sound handler is genuinely new here.
#include "scene/Scene.h"
#include "scene/Viewport.h"
#include "sound/Sound.h"
#include "sound/SoundHandler.h"
#include <math.h>


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PLAYER INSANITY COLLAPSE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerInsanityCollapse::cLuxPlayerInsanityCollapse(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerInsanityCollapse")
{
	mfHeightAddGoal = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_HeightAddGoal",0);
	
	mfHeightAddCollapseSpeed = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_HeightAddCollapseSpeed",0);
	mfHeightAddAwakeSpeed = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_HeightAddAwakeSpeed",0);
	mfRollCollapseSpeed = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_RollCollapseSpeed",0);
	mfRollAwakeSpeed = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_RollAwakeSpeed",0);

	mfSleepTime = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_SleepTime",0);

	mfSleepSpeedMul = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_SleepSpeedMul",0);
	mfWakeUpSpeedMul = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_WakeUpSpeedMul",0);

	// Voice sounds come from the player's own config (P2 = Justine's, when installed)
	msStartSound = mpPlayer->GetPlayerCfg()->GetString("Player_General","InsanityCollapse_StartSound", "");
	msAwakenSound = mpPlayer->GetPlayerCfg()->GetString("Player_General","InsanityCollapse_AwakenSound", "");
	msSleepLoopSound = mpPlayer->GetPlayerCfg()->GetString("Player_General","InsanityCollapse_SleepLoopSound", "");
	mfSleepLoopSoundVolume = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_SleepLoopSoundVolume", 0);

	msSleepRandomSound = mpPlayer->GetPlayerCfg()->GetString("Player_General","InsanityCollapse_SleepRandomSound", "");
	mfSleepRandomMinTime = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_SleepRandomMinTime", 0);
	mfSleepRandomMaxTime = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_SleepRandomMaxTime", 0);

	mfAwakenSanity = gpBase->mpGameCfg->GetFloat("Player_General","InsanityCollapse_AwakenSanity", 0);

	//Init sound var here
	mpLoopSound = NULL;
}

cLuxPlayerInsanityCollapse::~cLuxPlayerInsanityCollapse()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerInsanityCollapse::Reset()
{
	mbActive = false;
	mlState =0;
	mfHeightAdd =0;
	mfRoll =0;
	mfT=0;
	mfRandomCount=0;

	if(mpLoopSound && gpBase->mpEngine->GetSound()->GetSoundHandler()->IsValid(mpLoopSound, mlLoopSoundID))
	{
		mpLoopSound->Stop();
		mpLoopSound = NULL;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerInsanityCollapse::Start()
{
	if(mbActive) return;

	/////////////////
	//Play sound
	gpBase->mpHelpFuncs->PlayGuiSoundData(msStartSound, eSoundEntryType_World);

	/////////////////
	// Setup player
	mpPlayer->ChangeState(eLuxPlayerState_Normal);
	mpPlayer->ChangeMoveState(eLuxMoveState_Normal);
	mpPlayer->GetHelperLantern()->SetActive(false,false, false);
	mpPlayer->SetCurrentHandObjectDrawn(false);
	mpPlayer->SetInsanityCollapseSpeedMul(mfSleepSpeedMul);
	
	/////////////////
	//Set up sound
	cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	pSoundHandler->FadeGlobalVolume(0.75f, 0.15f, eSoundEntryType_World,eLuxGlobalVolumeType_InsanityCollapse, false);
	pSoundHandler->FadeGlobalSpeed(0.5f, 0.125f, eSoundEntryType_World,eLuxGlobalVolumeType_InsanityCollapse, false);

	/////////////////
	//Loop sound
	mpLoopSound = pSoundHandler->PlayGui(msSleepLoopSound,true,1.0f);
	if(mpLoopSound)
	{
		mpLoopSound->FadeIn(mfSleepLoopSoundVolume, 0.2f);
		mlLoopSoundID = mpLoopSound->GetId();
	}

	/////////////////
	//Attract enemies
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	cLuxEnemyIterator enemyIt = pMap->GetEnemyIterator();
	while(enemyIt.HasNext())
	{
		iLuxEnemy *pEnemy = enemyIt.Next();
        if(pEnemy->IsActive()) pEnemy->AlertOfPlayerPresence();
	}

	/////////////////
	//Setup variables.	
	mbActive = true;

	mlState =0;
	mfHeightAdd =0;
	mfRoll =0;
	mfT=0;
	mfRandomCount=0;
}

//-----------------------------------------------------------------------

void cLuxPlayerInsanityCollapse::Stop()
{
	if(mbActive==false) return;

	cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	pSoundHandler->FadeGlobalVolume(1, 0.3f, eSoundEntryType_World,eLuxGlobalVolumeType_InsanityCollapse, false);
	pSoundHandler->FadeGlobalSpeed(1, 0.5f, eSoundEntryType_World,eLuxGlobalVolumeType_InsanityCollapse, false);

	mpPlayer->SetInsanityCollapseSpeedMul(1.0f);
	
	if(mpLoopSound && pSoundHandler->IsValid(mpLoopSound, mlLoopSoundID))
	{
		mpLoopSound->FadeOut(1.0f);
	}

	mbActive = false;
	
	mlState =0;
	mfT =0;
	mfRoll =0;
	mfHeightAdd=0;
	mpPlayer->FadeRollTo(0, 10,10);
	mpPlayer->MoveHeadPosAdd(eLuxHeadPosAdd_InsanityCollapse, cVector3f(0,0,0),1, 0.1f);
}

//-----------------------------------------------------------------------

void cLuxPlayerInsanityCollapse::Update(float afTimeStep)
{
	if(mbActive==false) return;
	
	////////////////////////
	// Collapse
	if(mlState == 0 || mlState == 1)
	{
		//////////////////////
		// Height add
		if(mfHeightAdd > mfHeightAddGoal)
		{
			mfHeightAdd-= mfHeightAddCollapseSpeed*afTimeStep;
			if(mfHeightAdd < mfHeightAddGoal)
			{
				mfHeightAdd = mfHeightAddGoal;
				mlState = 1;
			}
			mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_InsanityCollapse, cVector3f(0,mfHeightAdd,0));
		}

		//////////////////////
		// Roll
		mfRoll += cMath::ToRad(mfRollCollapseSpeed)*afTimeStep;
		if(mfRoll > cMath::ToRad(35.0f)) mfRoll = cMath::ToRad(35.0f);

		mpPlayer->FadeRollTo(mfRoll, 10,10);
	}
	////////////////////////
	// Sleep
	if(mlState == 1)
	{
        mfT += afTimeStep;

		////////////////////////
		// Random sounds
		mfRandomCount -= afTimeStep;
		if(mfRandomCount <0)
		{
			gpBase->mpHelpFuncs->PlayGuiSoundData(msSleepRandomSound, eSoundEntryType_Gui);
			mfRandomCount = cMath::RandRectf(mfSleepRandomMinTime, mfSleepRandomMaxTime);
		}

		///////////////////////
		//Wake up
		if(mfT > mfSleepTime)
		{
			cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
			pSoundHandler->FadeGlobalVolume(1, 0.2f, eSoundEntryType_World,eLuxGlobalVolumeType_InsanityCollapse, false);
			pSoundHandler->FadeGlobalSpeed(1, 0.25f, eSoundEntryType_World,eLuxGlobalVolumeType_InsanityCollapse, false);

			mpPlayer->SetInsanityCollapseSpeedMul(mfWakeUpSpeedMul);

			mpPlayer->FadeRollTo(0, 5, 6);

			if(mpLoopSound && pSoundHandler->IsValid(mpLoopSound, mlLoopSoundID))
			{
				mpLoopSound->FadeOut(0.3f);
			}

			mpPlayer->SetSanity(mfAwakenSanity);

			gpBase->mpHelpFuncs->PlayGuiSoundData(msAwakenSound, eSoundEntryType_World);
			mlState =2;
		}
	}
	////////////////////////
	// Awake
	else if(mlState == 2)
	{
		//////////////////////
		// Height add
		if(mfHeightAdd < 0)
		{
			mfHeightAdd += mfHeightAddAwakeSpeed*afTimeStep;
			if(mfHeightAdd > 0)
			{
				mfHeightAdd = 0;
				mbActive = false;
				mpPlayer->SetInsanityCollapseSpeedMul(1.0f);
				mpPlayer->SetCrouchDisabled(false);
				mpPlayer->SetJumpDisabled(false);
			}
			mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_InsanityCollapse, cVector3f(0,mfHeightAdd,0));
		}
	}
}


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PLAYER SIGHT SWAY
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerCamDirEffects::cLuxPlayerCamDirEffects(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerCamDirEffects")
{
	mfStartSwayMaxSanity =  gpBase->mpGameCfg->GetFloat("Player_Sanity","StartSwayMaxSanity",0);
	mlMaxPositions = gpBase->mpGameCfg->GetInt("Player_Sanity","SwayMaxSavedPositions",0);
}

cLuxPlayerCamDirEffects::~cLuxPlayerCamDirEffects()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerCamDirEffects::Reset()
{
	mbSwayActive = false;

	mlstPrevAdd.clear();
	mvSwayAdd = 0;
	
	mvNextAdd = 0;

	mfSwayAlpha =0;
}

//-----------------------------------------------------------------------


void cLuxPlayerCamDirEffects::Update(float afTimeStep)
{
	////////////////////
	// Check if insane
	if(mbSwayActive==false && mpPlayer->GetSanity() <= mfStartSwayMaxSanity)
	{
		SetSwayActive(true);
	}
	else if(mbSwayActive && mpPlayer->GetSanity() > mfStartSwayMaxSanity)
	{
		SetSwayActive(false);
	}

	////////////////////
	// Alpha
	if(mbSwayActive && mfSwayAlpha<1)
	{
		mfSwayAlpha += afTimeStep *0.1f;
		if(mfSwayAlpha > 1) mfSwayAlpha =1;
	}
	else if(mbSwayActive==false && mfSwayAlpha>0)
	{
		mfSwayAlpha -= afTimeStep *0.2f;
		if(mfSwayAlpha < 0)
		{
			mfSwayAlpha =0;
			mlstPrevAdd.clear();
			mvNextAdd = 0;
			mvSwayAdd = 0;
		}
	}

	////////////////////
	// Check Sane
	UpdateSway(afTimeStep);
}

//-----------------------------------------------------------------------

float cLuxPlayerCamDirEffects::AddAndGetYawAdd(float afX)
{
	float fYaw = afX;
	if(mlstPrevAdd.empty()==false) fYaw = mvSwayAdd.x * mfSwayAlpha + afX *(1-mfSwayAlpha);

	if(mfSwayAlpha>0) mvNextAdd.x  = afX;
	return fYaw;
}

float cLuxPlayerCamDirEffects::AddAndGetPitchAdd(float afX)
{
	float fPitch = afX;
	if(mlstPrevAdd.empty()==false) fPitch = mvSwayAdd.y*mfSwayAlpha + afX *(1-mfSwayAlpha);

	if(mfSwayAlpha>0) mvNextAdd.y  = afX;
	return fPitch;
}

//-----------------------------------------------------------------------

void cLuxPlayerCamDirEffects::SetSwayActive(bool abX)
{
	if(mbSwayActive == abX) return;

	mbSwayActive = abX;
}

//-----------------------------------------------------------------------


void cLuxPlayerCamDirEffects::UpdateSway(float afTimeStep)
{
	if(mbSwayActive==false && mlstPrevAdd.empty() && mfSwayAlpha<=0) return;

	//////////////////////////////////
	//Add Yaw and Pitch and pop front is needed
	mlstPrevAdd.push_back(mvNextAdd);
	mvNextAdd =0;

	if((int)mlstPrevAdd.size() > mlMaxPositions)
	{
		mlstPrevAdd.pop_front();
	}
	
	////////////////////////////
	// Calculate the sway add
	cVector2f vTotal(0);
	tVector2fListIt it = mlstPrevAdd.begin();
	for(; it != mlstPrevAdd.end(); ++it)
	{
		vTotal += *it;
	}

	mvSwayAdd = vTotal/(float)mlstPrevAdd.size();
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER SPAWN PS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerSpawnPS::cLuxPlayerSpawnPS(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerSpawnPS")
{

}

cLuxPlayerSpawnPS::~cLuxPlayerSpawnPS()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::Reset()
{
	mbActive = false;
	msFileName = "";

	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap && mvSpawnPos.empty()==false)
	{
		cWorld *pWorld = pMap->GetWorld();
		for(size_t i=0; i<mvSpawnPos.size(); ++i)
		{
			cParticleSystem *pPS = mvSpawnPos[i].mpPS;

			if(pWorld->ParticleSystemExists(pPS))
				pWorld->DestroyParticleSystem(pPS);
		}
	}

	mvSpawnPos.clear();
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::Start(const tString& asFileName)
{
	if(LoadSpawnPSFile(asFileName)==false)
	{
		return;
	}

	msFileName = asFileName;
	mbActive = true;
	
	RespawnAll();
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::Stop()
{
	mbActive = false;
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::RespawnAll()
{
	DestroyAllSpawnPoints();
	GenerateAllSpawnPos();
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::Update(float afTimeStep)
{
	if(mbActive==false) return;

	cVector3f vPlayerPos = mpPlayer->GetCharacterBody()->GetFeetPosition();
	cWorld *pWorld = gpBase->mpMapHandler->GetCurrentMap()->GetWorld();

	for(size_t i=0; i<mvSpawnPos.size(); ++i)
	{
		cLuxPlayerSpawnPS_SpawnPos &spawnPos = mvSpawnPos[i];

		cVector3f vCurrentLocal = spawnPos.mvPos - vPlayerPos;

		///////////////////
		// Check if outside of "radius" (really a square)
		if(fabs(vCurrentLocal.x) > mfRadius || fabs(vCurrentLocal.z) > mfRadius)
		{
			////////////////////
			//Kill PS
			cParticleSystem *pPS = spawnPos.mpPS;
			if(pPS && pWorld->ParticleSystemExists(pPS))
			{
				pPS->Kill();
			}

			////////////////////
			//Get new position, opposite of the old
			cVector3f vAdd(0);
			cVector3f vMul(1);
			if(vCurrentLocal.x > mfRadius){			vAdd.x = vCurrentLocal.x - mfRadius;	vMul.x=-1; }
			else if(vCurrentLocal.x < -mfRadius){	vAdd.x = vCurrentLocal.x + mfRadius;	vMul.x=-1; }
			if(vCurrentLocal.z > mfRadius){			vAdd.z = vCurrentLocal.z - mfRadius;	vMul.z=-1; }
			else if(vCurrentLocal.z < -mfRadius){	vAdd.z = vCurrentLocal.z + mfRadius;	vMul.z=-1; }

			cVector3f vTemp = vCurrentLocal;
			vCurrentLocal = vCurrentLocal*vMul + vAdd*2;//*2=first remove the offset and then put it inside.

			//Still do random height!
			vCurrentLocal.y = vPlayerPos.y;
			vCurrentLocal.y += mfHeightFromFeet + cMath::RandRectf(mfHeightAddMin, mfHeightAddMax);

			spawnPos.mvPos = vPlayerPos + vCurrentLocal;

			////////////////////
			//Create Particle System
			spawnPos.mpPS = CreatePS(&spawnPos);
		}
		///////////////////
		// Update the local pos
		else
		{
			spawnPos.mvLastLocalPos = vCurrentLocal;
		}
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::GenerateAllSpawnPos()
{
	cVector3f vPlayerPos = mpPlayer->GetCharacterBody()->GetFeetPosition();

	cVector3f vStep = cVector3f(1/mfDensity, 0, 1/mfDensity);
	cVector3f vLocalStartPos = cVector3f(-mfRadius, 0, -mfRadius) + vStep*0.5f;
	cVector3f vLocalPos = vLocalStartPos;

	for(; vLocalPos.z < mfRadius; vLocalPos.z += vStep.z)
	{
		vLocalPos.x = vLocalStartPos.x;

		for(; vLocalPos.x < mfRadius; vLocalPos.x += vStep.x)
		{
			cLuxPlayerSpawnPS_SpawnPos spawnPos;

			float fRandRange = (1.0f/mfDensity)*0.2f;
			cVector3f vFinalLocalPos = vLocalPos +	cMath::RandRectVector3f(cVector3f(-fRandRange,0,-fRandRange),
																			cVector3f(fRandRange,0,fRandRange));
			vFinalLocalPos.y += mfHeightFromFeet + cMath::RandRectf(mfHeightAddMin, mfHeightAddMax);

			spawnPos.mvLastLocalPos = vFinalLocalPos;
			spawnPos.mvPos = vPlayerPos + vFinalLocalPos;

			spawnPos.mpPS = CreatePS(&spawnPos);
			if(spawnPos.mpPS)
				mvSpawnPos.push_back(spawnPos);
		}
	}
}

//-----------------------------------------------------------------------

cParticleSystem* cLuxPlayerSpawnPS::CreatePS(cLuxPlayerSpawnPS_SpawnPos *apSpawnPos)
{
	cWorld *pWorld = gpBase->mpMapHandler->GetCurrentMap()->GetWorld();
	cParticleSystem *pPS = pWorld->CreateParticleSystem("SpawnPS", msParticleSystem, 1);
	if(pPS)
	{
		pPS->SetPosition(apSpawnPos->mvPos);
		pPS->SetFadeAtDistance(mbFadePS);
		pPS->SetMinFadeDistanceStart(mfPSMinFadeStart);
		pPS->SetMinFadeDistanceEnd(mfPSMinFadeEnd);
		pPS->SetMaxFadeDistanceStart(mfPSMaxFadeStart);
		pPS->SetMaxFadeDistanceEnd(mfPSMaxFadeEnd);
		pPS->SetColor(mPSColor);
	}
	return pPS;
}

//-----------------------------------------------------------------------

void cLuxPlayerSpawnPS::DestroyAllSpawnPoints()
{
	cWorld *pWorld = gpBase->mpMapHandler->GetCurrentMap()->GetWorld();
	for(size_t i=0; i<mvSpawnPos.size(); ++i)
	{
		//Detroy particle system
		cLuxPlayerSpawnPS_SpawnPos &spawnPos = mvSpawnPos[i];
		cParticleSystem *pPS = spawnPos.mpPS;
		if(pPS && pWorld->ParticleSystemExists(pPS))
		{
			pPS->Kill();
		}		
	}
	mvSpawnPos.clear();
}

//-----------------------------------------------------------------------

bool cLuxPlayerSpawnPS::LoadSpawnPSFile(const tString& asFileName)
{
	tString sFile = cString::SetFileExt(asFileName, "sps");
	cResources *pResources = gpBase->mpEngine->GetResources();

	iXmlDocument *pXmlDoc = pResources->LoadXmlDocument(sFile);
	if(pXmlDoc==NULL)
	{
		Error("Could not load sps file: '%s'\n", sFile.c_str());
		return false;
	}

	msParticleSystem = pXmlDoc->GetAttributeString("ParticleSystem","");
	mfHeightFromFeet = pXmlDoc->GetAttributeFloat("HeightFromFeet",0);
	mfHeightAddMin = pXmlDoc->GetAttributeFloat("HeightAddMin",0);
	mfHeightAddMax = pXmlDoc->GetAttributeFloat("HeightAddMax",0);
	mfDensity = pXmlDoc->GetAttributeFloat("Density",0);
	mfRadius = pXmlDoc->GetAttributeFloat("Radius",0);
	mPSColor = pXmlDoc->GetAttributeColor("PSColor",cColor(0));
	mbFadePS = pXmlDoc->GetAttributeBool("FadePS",true);
	mfPSMinFadeStart = pXmlDoc->GetAttributeFloat("PSMinFadeStart",0);
	mfPSMinFadeEnd = pXmlDoc->GetAttributeFloat("PSMinFadeEnd",0);
	mfPSMaxFadeStart = pXmlDoc->GetAttributeFloat("PSMaxFadeStart",0);
	mfPSMaxFadeEnd = pXmlDoc->GetAttributeFloat("PSMaxFadeEnd",0);
	
	pResources->DestroyXmlDocument(pXmlDoc);

	return true;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER HURT
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerHurt::cLuxPlayerHurt(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerHurt")
{
	mfEffectStartHealth =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_EffectStartHealth",0);
	mfMinMoveMul =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_MinSpeedMul",0);

	mfMaxPantCount =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_MaxPantCount",0);
	mfPantSpeed =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_PantSpeed",0);
	mfPantSize =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_PantSize",0);

	mfHealthRegainSpeed =  gpBase->mpGameCfg->GetFloat("Player_General","HealthRegainSpeed",0);
	mfHealthRegainLimit =  gpBase->mpGameCfg->GetFloat("Player_General","HealthRegainLimit",0);

	mfNoiseAlpha =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_NoiseAlpha",0);
	mfNoiseFreq =  gpBase->mpGameCfg->GetFloat("Player_General","Hurt_NoiseFreq",0);
	mNoiseColor =  gpBase->mpGameCfg->GetColor("Player_General","Hurt_NoiseColor",cColor(0));

	cGui *pGui = gpBase->mpEngine->GetGui();
	mvNoiseGfx.resize(8);
	for(size_t i=0; i<mvNoiseGfx.size(); ++i)
	{
		mvNoiseGfx[i] = pGui->CreateGfxTexture("hud_hurt_noise0"+cString::ToString((int)i)+".dds", eGuiMaterial_Modulative,eTextureType_2D);
	}
	
	//mpWhiteGfx = gpBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1,1),eGuiMaterial_Modulative);
}

cLuxPlayerHurt::~cLuxPlayerHurt()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerHurt::Reset()
{
	mfAlpha = 0;
	mfPantCount =0;
	mfPantPosAdd = 0;
	mfPantPosAddVel = 0;
	mfPantPosAddDir = 1.0f;
	mlCurrentNoise = 0;
	mfNoiseUpdateCount =0;
}

//-----------------------------------------------------------------------

void cLuxPlayerHurt::Update(float afTimeStep)
{
	///////////////////////////
	// Health regain
	if(mpPlayer->GetHealth() < mfHealthRegainLimit)
	{
		mpPlayer->AddHealth(mfHealthRegainSpeed * afTimeStep);
	}

	////////////////////////////
	// Check if update is needed
	if(mfAlpha <=0 && mpPlayer->GetHealth() > mfEffectStartHealth) return;
	if(mpPlayer->IsDead()) return;

	float fWantedAlpha = 1 - (mpPlayer->GetHealth() / mfEffectStartHealth);
	if(fWantedAlpha > 1) fWantedAlpha = 1;
	if(fWantedAlpha < 0) fWantedAlpha = 0;

	//////////////////////////////
	// Alpha
	if(mfAlpha < fWantedAlpha)
	{
		mfAlpha += afTimeStep;
		if(mfAlpha > fWantedAlpha) mfAlpha = fWantedAlpha;
	}
	else if(mfAlpha > fWantedAlpha)
	{
		mfAlpha -= afTimeStep;
		if(mfAlpha < fWantedAlpha) mfAlpha = fWantedAlpha;
	}

	mpPlayer->SetHurtMoveSpeedMul(mfMinMoveMul*mfAlpha + (1-mfAlpha) );
	
	//////////////////////////////
	// Noise
	mfNoiseUpdateCount -= afTimeStep;
	if(mfNoiseUpdateCount<=0)
	{
		int lNoiseMax = (int)mvNoiseGfx.size()-1;
		mlCurrentNoise += cMath::RandRectl(1, lNoiseMax);
		if(mlCurrentNoise>lNoiseMax) mlCurrentNoise -= lNoiseMax+1;

		mfNoiseUpdateCount = 1.0f / mfNoiseFreq;
	}

	//////////////////////////////
	// Update pant count
	iCharacterBody *pCharBody =mpPlayer->GetCharacterBody();
	float fSpeed = pCharBody->GetVelocity(afTimeStep).Length();
	if(fSpeed < 0.05f)
	{
		if(mfPantCount > 0)
		{
			mfPantCount -= afTimeStep;
		}
	}
	else
	{
		mfPantCount += afTimeStep;

		float fMax = mfMaxPantCount * mfAlpha;
		if(mfPantCount > fMax) mfPantCount = fMax;
	}


	//////////////////////////////
	// Update panting
	if(mfPantCount > 0 && fSpeed < 0.05f)
	{
		mfPantPosAddVel += mfPantPosAddDir * afTimeStep;
		mfPantPosAddVel = cMath::Clamp(mfPantPosAddVel, -1, 1);

		mfPantPosAdd += afTimeStep * mfPantPosAddVel * mfPantSpeed;
		if(mfPantPosAddDir > 0)
		{
			if(mfPantPosAdd > mfPantSize){
				mfPantPosAddDir = -mfPantPosAddDir;
				gpBase->mpHelpFuncs->PlayGuiSoundData(mpPlayer->ResolveVoiceSound("hurt_pant"),eSoundEntryType_Gui, cMath::Min(mfAlpha+0.3f, 1.0f) );
			}
		}
		else
		{
			if(mfPantPosAdd < -mfPantSize*0.5f){
				mfPantPosAddDir = -mfPantPosAddDir;
			}
		}
	}
	else if(mfPantPosAdd != 0)
	{
		float fDir = mfPantPosAdd > 0 ? -1.0f : 1.0f;
		mfPantPosAddVel += fDir * afTimeStep;

		mfPantPosAddDir = 1;

		float fMax = cMath::Abs(mfPantPosAdd / mfPantSize);
		mfPantPosAddVel = cMath::Clamp(mfPantPosAddVel, -fMax, fMax);

		mfPantPosAdd += afTimeStep * mfPantPosAddVel * mfPantSpeed;

		if( (fDir > 0 && mfPantPosAdd >0) || (fDir < 0 && mfPantPosAdd <0) )
		{
			mfPantPosAdd =0;
		}
	}


	mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_Hurt, cVector3f(0,mfPantPosAdd,0));

}

//-----------------------------------------------------------------------

void cLuxPlayerHurt::OnDraw(float afFrameTime)
{
	if(mfAlpha <=0) return;

	cGuiSet *pHurtSet = GetDrawHudSet();
	if(pHurtSet==NULL) return;	//P2 with no coop hud set this frame -- draw nowhere, not on P1.

	cVector2f vNoiseSize(256, 256);
	
	cVector2l vCount = cVector2l( (int)(gpBase->mvHudVirtualSize.x / vNoiseSize.x)+1, (int)(gpBase->mvHudVirtualSize.y / vNoiseSize.y)+1);

	cVector3f vPos = gpBase->mvHudVirtualStartPos + cVector3f(0,0,-1);

	cColor col = mNoiseColor;
	col.a = mfAlpha*mfNoiseAlpha;
	for(int y=0; y<vCount.y; ++y)
	{
		for(int x=0; x<vCount.x; ++x)
		{
			pHurtSet->DrawGfx(mvNoiseGfx[mlCurrentNoise], vPos, vNoiseSize, cColor(1,1-mfAlpha, 1-mfAlpha));
			vPos.x += vNoiseSize.x;
		}
		vPos.x = gpBase->mvHudVirtualStartPos.x;
		vPos.y += vNoiseSize.y;
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PLAYER FLASHBACK
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerFlashback::cLuxPlayerFlashback (cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerFlashback")
{
	mfRadialBlurSize = gpBase->mpGameCfg->GetFloat("Player_General","FlashbackRadialBlurSize", 0.12f);
	mfRadialBlurStartDist = gpBase->mpGameCfg->GetFloat("Player_General","FlashbackRadialBlurStartDist", 0.4f);
	mfWorldSoundVolume = gpBase->mpGameCfg->GetFloat("Player_General","FlashbackWorldSoundVolume", 0.4f);
	mfMoveSpeedMul = gpBase->mpGameCfg->GetFloat("Player_General","FlashbackMoveSpeedMul", 1.0f);
	mfRunSpeedMul = gpBase->mpGameCfg->GetFloat("Player_General","FlashbackRunSpeedMul", 1.0f);

	Reset();

}
cLuxPlayerFlashback::~cLuxPlayerFlashback ()
{
}

//-----------------------------------------------------------------------

void cLuxPlayerFlashback::Reset()
{
	mfFlashDelay =0;
	mbActive = false;
	mlstFlashbackQueue.clear();
}

//-----------------------------------------------------------------------

void cLuxPlayerFlashback::Start(const tString &asFlashbackFile, const tString &asCallback)
{
	if(gpBase->mpDebugHandler->GetDisableFlashBacks()) return;

	ProgLog(eLuxProgressLogLevel_Medium, "Starting flashback "+ asFlashbackFile);
	
	if(mbActive)
	{
		mlstFlashbackQueue.push_back(cLuxFlashbackData(asFlashbackFile, asCallback) );
		return;
	}

	//Disable enemies
	gpBase->mpMapHandler->GetCurrentMap()->BroadcastEnemyMessage(eLuxEnemyMessage_Reset, false,0,0);

	mfFlashDelay = 0.5f; //Show flash effect after a little delay
	gpBase->mpHelpFuncs->PlayGuiSoundData("flashback_flash", eSoundEntryType_Gui);
	
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	pMap->AddCompletionAmount(gpBase->mpCompletionCountHandler->mlFlashbackCompletionValue, 2.5f);

	mfFlashbackStartCount = 1.5f;
	msFlashbackFile = asFlashbackFile;
	msCallback = asCallback;

	//World sounds
	gpBase->mpEngine->GetSound()->GetSoundHandler()->FadeGlobalVolume(mfWorldSoundVolume, (1-mfWorldSoundVolume) / 1.5f,eSoundEntryType_World,eLuxGlobalVolumeType_Flashback,false);

	mbActive = true;
}

//-----------------------------------------------------------------------

void cLuxPlayerFlashback::Update(float afTimeStep)
{
	if(mbActive==false)
	{
		if(mlstFlashbackQueue.empty()==false)
		{
			mfFlashbackStartCount -= afTimeStep;
			if(mfFlashbackStartCount < 0)
			{
				cLuxFlashbackData data = mlstFlashbackQueue.front();
				mlstFlashbackQueue.pop_front();

				Start(data.msFile, data.msCallback);
			}
		}

		return;
	}

	////////////////////////////////
	// Flash effect (showed after small delay)
	if(mfFlashDelay > 0)
	{
		mfFlashDelay -= afTimeStep;
		if(mfFlashDelay < 0)
		{
			gpBase->mpEffectHandler->GetFlash()->Start(0.5f, 0.5f, 2.5f);

			//Apply to the player this helper belongs to, not to whoever gpBase->mpPlayer
			//currently points at (the codebase swaps that pointer during P2 interactions).
			mpPlayer->SetEventMoveSpeedMul(mfMoveSpeedMul);
			mpPlayer->SetEventRunSpeedMul(mfRunSpeedMul);

			//Coop: the vision is shown to both players, so slow both of them.
			if(gpBase->mpMapHandler->GetCoopMode() && gpBase->mpPlayer2 && gpBase->mpPlayer2!=mpPlayer &&
				ImGuiDebugMenu::GetSharedFlashbackVisions())
			{
				gpBase->mpPlayer2->SetEventMoveSpeedMul(mfMoveSpeedMul);
				gpBase->mpPlayer2->SetEventRunSpeedMul(mfRunSpeedMul);
			}
		}
	}

    ////////////////////////////////
	// Start voices and effects
	if(mfFlashbackStartCount>0)
	{
		mfFlashbackStartCount -= afTimeStep;
		if(mfFlashbackStartCount <= 0)
		{
			//Sepia
			gpBase->mpEffectHandler->GetSepiaColor()->FadeTo(1, 1.0f / 3.5f);

			//Radial blur -- the flashback is this player's, and so is the blur.
			//Shared Flashback Visions, when it is on, starts the sequence on the
			//other player too, and theirs claims it in their own turn through here.
			gpBase->mpEffectHandler->SetRadialBlurOwner(mpPlayer);
			gpBase->mpEffectHandler->GetRadialBlur()->SetBlurStartDist(mfRadialBlurStartDist);
			gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(mfRadialBlurSize, mfRadialBlurSize / 3.5f);
			
			//Play voices
			LoadAndPlayFlashbackFile(msFlashbackFile);
		}
	}
	////////////////////////////////
	// Update effects and check if over
	else
	{
		//TODO: Effects?

		/////////////////////////////////////
		//Are the voices done playing? (note that effects might still be playing, but flash ends when voceis are done!)
		if(gpBase->mpEffectHandler->GetPlayVoice()->VoiceDonePlaying())
		{
			float fFadeTime = 4.0f;
			//Flash at end
			gpBase->mpEffectHandler->GetFlash()->Start(1.0f, 0.1f, 2.5f);
			
			//Post effects
			gpBase->mpEffectHandler->GetSepiaColor()->FadeTo(0, 1.0f / fFadeTime);
			gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(0, mfRadialBlurSize / fFadeTime);

			//World sounds
			gpBase->mpEngine->GetSound()->GetSoundHandler()->FadeGlobalVolume(1, 1 / fFadeTime,eSoundEntryType_World,eLuxGlobalVolumeType_Flashback,true);
			
			mpPlayer->SetEventMoveSpeedMul(1.0f);
			mpPlayer->SetEventRunSpeedMul(1.0f);

			//Coop: restore the other player too (see the matching block above).
			if(gpBase->mpMapHandler->GetCoopMode() && gpBase->mpPlayer2 && gpBase->mpPlayer2!=mpPlayer &&
				ImGuiDebugMenu::GetSharedFlashbackVisions())
			{
				gpBase->mpPlayer2->SetEventMoveSpeedMul(1.0f);
				gpBase->mpPlayer2->SetEventRunSpeedMul(1.0f);
			}

			mbActive = false;

			mfFlashbackStartCount = 6.0f; //Incase there is something in queue, wait 6 seconds and then start that.

			cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
			if(msCallback != "")
				pMap->RunScript(msCallback + "()");
		}
	}
	
}

//-----------------------------------------------------------------------

void cLuxPlayerFlashback::OnDraw(float afFrameTime)
{
}

//-----------------------------------------------------------------------

void cLuxPlayerFlashback::LoadAndPlayFlashbackFile(const tString& asFlashbackFile)
{
	tString sFile = cString::SetFileExt(asFlashbackFile,"flash");
	cResources *pResources = gpBase->mpEngine->GetResources();

	iXmlDocument *pXmlDoc = pResources->LoadXmlDocument(sFile);
	if(pXmlDoc==NULL)
	{
		Error("Could not load flashback file: '%s'\n", sFile.c_str());
		return;
	}

	cXmlElement *pVoicesElem = pXmlDoc->GetFirstElement("Voices");
	if(pVoicesElem==NULL)
	{
		Error("Could not find voice element in flashback file '%s'\n", sFile.c_str());
		pResources->DestroyXmlDocument(pXmlDoc);
		return;
	}

	cXmlNodeListIterator it = pVoicesElem->GetChildIterator();
	while(it.HasNext())
	{
		cXmlElement *pChildElem = it.Next()->ToElement();

		tString sVoiceFile = pChildElem->GetAttributeString("VoiceSound","");
		tString sEffectFile = pChildElem->GetAttributeString("EffectSound","");
		tString sTextCat = pChildElem->GetAttributeString("TextCat","");
		tString sTextEntry = pChildElem->GetAttributeString("TextEntry","");

		gpBase->mpEffectHandler->GetPlayVoice()->AddVoice(sVoiceFile, sEffectFile, sTextCat, sTextEntry, false,0,0,0);
	}

	pResources->DestroyXmlDocument(pXmlDoc);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER HEAD MOVE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerLookAt::cLuxPlayerLookAt(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerLookAt")
{
	Reset();	
}

//-----------------------------------------------------------------------


cLuxPlayerLookAt::~cLuxPlayerLookAt()
{
}

//-----------------------------------------------------------------------

void cLuxPlayerLookAt::Update(float afTimeStep)
{
	if(mbActive==false) return;

	cCamera *pCam = mpPlayer->GetCamera();
	cVector3f vGoalAngle = cMath::GetAngleFromPoints3D(pCam->GetPosition(),mvTargetPos);

	///////////////////////////
	//Get distance to goal
	cVector3f vDist; 
	vDist.x = cMath::GetAngleDistanceRad(pCam->GetPitch(),vGoalAngle.x);
	vDist.y = cMath::GetAngleDistanceRad(pCam->GetYaw(),vGoalAngle.y);

	///////////////////////////
	//Get the Speed
	cVector3f vWantedSpeed;
	vWantedSpeed.x = cMath::Min(vDist.x * mfSpeedMul, mfMaxSpeed);
	vWantedSpeed.y = cMath::Min(vDist.y * mfSpeedMul, mfMaxSpeed);

	cVector3f vSpeedDiff = vWantedSpeed - mvCurrentSpeed;
	mvCurrentSpeed += vSpeedDiff*afTimeStep*10;

	//Add Pitch
	pCam->AddPitch(mvCurrentSpeed.x * afTimeStep);

	//Add yaw
	pCam->AddYaw(mvCurrentSpeed.y * afTimeStep);
	mpPlayer->GetCharacterBody()->SetYaw(pCam->GetYaw()); 

	float fTotalDist = vDist.x*vDist.x + vDist.y*vDist.y;
	if(fTotalDist < 0.01)
	{
		gpBase->mpMapHandler->GetCurrentMap()->RunScript(msAtTargetCallback+"()");
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerLookAt::Reset()
{
	mbActive = false;
	mfMaxSpeed = 9999.0f;
	mfSpeedMul = 1.0f;
	mvTargetPos = cVector3f(0,1,0);
	mvCurrentSpeed =0;

	msAtTargetCallback = "";

	mfDestFovMul = 1.0f;
	mfFov = 1.0f;
	mfFovSpeed = 1.0f;
	mfFovMaxSpeed = 1.0f;
}

//-----------------------------------------------------------------------

void cLuxPlayerLookAt::SetTarget(const cVector3f &avTargetPos, float afSpeedMul, float afMaxSpeed, const tString& asAtTargetCallback)
{
	mvTargetPos = avTargetPos;
	mfSpeedMul = afSpeedMul;
	mfMaxSpeed = afMaxSpeed;

	msAtTargetCallback = asAtTargetCallback;
}

//-----------------------------------------------------------------------

void cLuxPlayerLookAt::SetActive(bool abX)
{
	mbActive = abX;
	if(mbActive==false)
	{
		mvCurrentSpeed =0;
	}
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER SANITY
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerSanity::cLuxPlayerSanity(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerSanity")
{
	mfHitZoomInSpeed = gpBase->mpGameCfg->GetFloat("Player_Sanity","HitZoomInSpeed",0);
	mfHitZoomOutSpeed = gpBase->mpGameCfg->GetFloat("Player_Sanity","HitZoomOutSpeed",0);
	mfHitZoomInFOVMul = gpBase->mpGameCfg->GetFloat("Player_Sanity","HitZoomInFOVMul",0);
	mfHitZoomInAspectMul = gpBase->mpGameCfg->GetFloat("Player_Sanity","HitZoomInAspectMul",0);

	mfSanityRegainSpeed = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityRegainSpeed",0);
	mfSanityRegainLimit = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityRegainLimit",0);

	mfSanityVeryLowLimit = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityVeryLowLimit",0);
	mfSanityEffectsStart = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityEffectsStart",0);

	mfSanityWaveAlphaMul = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityWaveAlphaMul",0);
	mfSanityWaveSpeedMul = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityWaveSpeedMul",0);

	mfSanityLowLimit = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityLowLimit",0);
	mfSanityLowLimitMaxTime = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityLowLimitMaxTime",0);
	mfSanityLowNewSanityAmount = gpBase->mpGameCfg->GetFloat("Player_Sanity","SanityLowNewSanityAmount",0);

	mfCheckNearEnemyInterval = gpBase->mpGameCfg->GetFloat("Player_Sanity","CheckNearEnemyInterval",0);
	mfNearEnemyDecrease = gpBase->mpGameCfg->GetFloat("Player_Sanity","NearEnemyDecrease",0);
	mfNearCritterDecrease = gpBase->mpGameCfg->GetFloat("Player_Sanity","NearCritterDecrease",0);

	Reset();
}

//-----------------------------------------------------------------------

cLuxPlayerSanity::~cLuxPlayerSanity()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::Reset()
{
	mfHitAlpha =0;
	mbHitActive = false;
	mfSanityLostCount =0;
	mfPantCount =1;
	mfCheckEnemySeenCount =0;

	mfT=0;
	mfInsaneWaveAlpha =0;

	mfSanityDrainCount =0;
	mfSanityDrainVolume =0;
	mfSanityHeartbeatCount =0;

	mfSeenEnemyCount =0;
	mbEnemyIsSeen = false;

	mbSanityEffectUpdated = false;

	mfAtLowSanityCount =0;

	mfShowHintTimer =0;
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::StartHit()
{
	mbHitActive = true;

	gpBase->mpHelpFuncs->PlayGuiSoundData(mpPlayer->ResolveVoiceSound("sanity_damage"), eSoundEntryType_Gui);
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::SetSanityLost()
{
	mfSanityLostCount = 1.0f;
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::Update(float afTimeStep)
{
	mfT += afTimeStep;

	mbHitIsUpdated = false;
	mbSanityLostIsUpdated = false;

	//////////////////////
	// Check if player is at low sanity level and update a timer
	if(mpPlayer->GetSanity() < mfSanityLowLimit)
	{
		mfAtLowSanityCount+=afTimeStep;
		if(mfAtLowSanityCount > mfSanityLowLimitMaxTime)
		{
			mfAtLowSanityCount =0;
			mpPlayer->SetSanity(mfSanityLowNewSanityAmount);
		}
	}
	else if(mfAtLowSanityCount >0)
	{
		mfAtLowSanityCount -= afTimeStep;
		if(mfAtLowSanityCount <0) mfAtLowSanityCount =0;
	}

	//////////////////////
	// Update complex stuff
	UpdateCheckEnemySeen(afTimeStep);
	UpdateEnemySeenEffect(afTimeStep);
	UpdateHit(afTimeStep);
	UpdateInsaneEffects(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::OnDraw(float afFrameTime)
{
	
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::UpdateInsanityVisuals(float afTimeStep)
{
	////////////////////////////////////////////////////////////////////////
	// Drive THIS player's insanity instance -- in split coop that is the one
	// on this player's own composite (the shared main-viewport instance is
	// hidden there, and two players writing one instance meant the last
	// writer's sanity decided both screens). NULL when this player has no
	// instance to drive (P2 with the coop post pass off): drive nothing.
	cLuxPostEffect_Insanity *pInsanity = gpBase->mpMapHandler->GetInsanityEffectForPlayer(mpPlayer);
	if(pInsanity == NULL) return;

	if(mpPlayer->GetSanity() > mfSanityEffectsStart && mfSanityDrainVolume <=0 && mfInsaneWaveAlpha <=0)
	{
		pInsanity->SetActive(false);
		return;
	}

	pInsanity->SetActive(true);

	float fSanity = mpPlayer->GetSanity();
	float fGoalAlpha = 1 - fSanity / mfSanityEffectsStart;
	if(fGoalAlpha < 0) fGoalAlpha =0;

	////////////////////////////////
	//Update wave alpha
	if(fGoalAlpha < mfInsaneWaveAlpha)
	{
		mfInsaneWaveAlpha -= afTimeStep;
		if(mfInsaneWaveAlpha < fGoalAlpha) mfInsaneWaveAlpha = fGoalAlpha;
	}
	else
	{
		mfInsaneWaveAlpha += afTimeStep;
		if(mfInsaneWaveAlpha > fGoalAlpha) mfInsaneWaveAlpha = fGoalAlpha;
	}

	////////////////////////////////
	//Set up effects
	//Log("Zoom: %f Wave: %f\n", mfSanityDrainVolume, mfInsaneWaveAlpha);
	
	float fZoomMul = (sin(mfT*2)+1)*0.5f*0.4f + 0.6f; 

	pInsanity->SetWaveAlpha(mfInsaneWaveAlpha * mfSanityWaveAlphaMul);//mfInsaneWaveAlpha);
	pInsanity->SetWaveSpeed(mfInsaneWaveAlpha * mfSanityWaveSpeedMul);//*mfInsaneWaveAlpha);
	pInsanity->SetZoomAlpha(mfSanityDrainVolume * fZoomMul);
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::UpdateInsaneEffects(float afTimeStep)
{
	if(mbHitIsUpdated) return;

	if(mbSanityEffectUpdated)
	{
		gpBase->mpEffectHandler->GetImageTrail()->FadeTo(0, 1);
		mpPlayer->FadeAspectMulTo(1, 1);
		mpPlayer->FadeFOVMulTo(1, 1);

		mbSanityEffectUpdated = false;
	}

	////////////////////////////////
	// Insanity visual effect
	UpdateInsanityVisuals(afTimeStep);

	if(mpPlayer->IsDead()) return;
	////////////////////////////////
	// Player is loosing sanity!
	UpdateLosingSanity(afTimeStep);

	////////////////////////////////
	// Show that sanity is low
	UpdateLowSanity(afTimeStep);

	////////////////////////////////
	// Regain some sanity
	float fSanity = mpPlayer->GetSanity();
	if(fSanity < mfSanityRegainLimit)
	{
		fSanity += afTimeStep * mfSanityRegainSpeed;
		mpPlayer->SetSanity(fSanity);
	}

}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::UpdateCheckEnemySeen(float afTimeStep)
{
	/////////////////////////////////////
	// Check if it is time for a check!
	if(mfCheckEnemySeenCount >0)
	{
		mfCheckEnemySeenCount-=afTimeStep;
		return;
	}
	mfCheckEnemySeenCount = mfCheckNearEnemyInterval;

	/////////////////////////////////////
	// Init vars
	bool bSeenEnemy = false;
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	float fMaxRangeSqrt = 35 * 35;

	cCamera *pCam = mpPlayer->GetCamera();
	cVector3f vPlayerHeadPos = pCam->GetPosition();


	cVector3f vRight = pCam->GetRight();
	cVector3f vUp = pCam->GetUp();
	
	/////////////////////////////////////
	// Iterate critters
	float fMinCritterDistSqrt = 3.0f * 3.0f;
	bool bNearCritter = false;
	cLuxEntityIterator entIt = pMap->GetEntityIterator();
	while(entIt.HasNext())
	{
		iLuxEntity *pEntity = entIt.Next();
		if(pEntity->IsActive()==false)continue;
		if(pEntity->GetEntityType() != eLuxEntityType_Prop) continue;

		iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
		if(pProp->GetPropType() != eLuxPropType_Critter) continue;

		iLuxProp_CritterBase *pCritter = static_cast<iLuxProp_CritterBase*>(pProp);
		
		if(pCritter->CausesSanityDecrease()==false) continue;
		
		float fDistSqrt = cMath::Vector3DistSqr(pProp->GetBody(0)->GetLocalPosition(), vPlayerHeadPos); 
		if(fDistSqrt > fMinCritterDistSqrt) continue;

		bNearCritter = true;
		break;
	}

	if(bNearCritter)
	{
		mpPlayer->LowerSanity(mfNearCritterDecrease, true);
	}
	
	/////////////////////////////////////
	// Iterate enemies
	cLuxEnemyIterator it = pMap->GetEnemyIterator();
    while(it.HasNext())
	{
		iLuxEnemy *pEnemy = it.Next();
		pEnemy->SetIsSeenByPlayer(false);
		
		if(pEnemy->IsActive()==false) continue;
		if(pEnemy->CausesSanityDecrease()==false) continue;

		iCharacterBody *pCharBody = pEnemy->GetCharacterBody();

		//////////////////////////////
		//Check so enemy is in range
		float fDistSqrt = cMath::Vector3DistSqr(pCharBody->GetPosition(), vPlayerHeadPos);
		if(fDistSqrt > fMaxRangeSqrt) continue;
		
		//////////////////////////////
		//Check so enemy is in FOV
		if( pCam->GetFrustum()->CollideBoundingVolume(pCharBody->GetCurrentBody()->GetBoundingVolume()) == eCollision_Outside)
		{
			continue;
		}
		
		//////////////////////////////
		//Cast rays
		cVector3f vHalfSize = pCharBody->GetSize()*0.5f;
		cVector3f vPosAdd[5] = {
			cVector3f(0),
			vRight*vHalfSize.x,
			vRight*vHalfSize.x*-1,
			vUp*vHalfSize.y*0.8f,
			vUp*vHalfSize.y*-0.8f,
		};

		int lCount =0;
		for(int i=0; i<5; ++i)
		{
			if(gpBase->mpMapHelper->CheckLineOfSight(vPlayerHeadPos, pCharBody->GetPosition()+vPosAdd[i], false))
			{
				lCount++;
				if(lCount >=2)
				{
					bSeenEnemy = true;
					pEnemy->SetIsSeenByPlayer(true);
					break;
				}
			}
		}
		
		//if(bSeenEnemy) break; No break, since we check visibility for all enemies.
	}

	/////////////////////////////////////
	// If seen, lower sanity and increase seen count
	if(bSeenEnemy)
	{
      	mpPlayer->LowerSanity(mfNearEnemyDecrease, true);
		
		//do this in update instead!
		//gpBase->mpEffectHandler->GetRadialBlur()->SetBlurStartDist(0.3f);
		//gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(0.12f, 0.12f / 3.0f);

		mbEnemyIsSeen = true;
	}
	else
	{
		if(mbEnemyIsSeen)
		{
			mbEnemyIsSeen = false;

			//Still theirs while it fades out, or the last half second of it would
			//jump onto the other player's screen on the way down.
			gpBase->mpEffectHandler->SetRadialBlurOwner(mpPlayer);
			gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(0, 0.12f / 2.0f);
		}
	}
	
}

//-----------------------------------------------------------------------


void cLuxPlayerSanity::UpdateHit(float afTimeStep)
{
	if(mpPlayer->IsDead()) return;
	if(mfHitAlpha<=0 && mbHitActive==false) return;

	mbHitIsUpdated = true;
	
	if(mbHitActive)
	{
		mfHitAlpha += afTimeStep * mfHitZoomInSpeed;
		if(mfHitAlpha >= 1)
		{
			mfHitAlpha =1;
			mbHitActive = false;
		}
	}
	else
	{
		mfHitAlpha -= afTimeStep * mfHitZoomOutSpeed;
		if(mfHitAlpha < 0) mfHitAlpha =0;
	}

	gpBase->mpEffectHandler->GetImageTrail()->FadeTo(mfHitAlpha * 0.9f, 100);
	mpPlayer->FadeAspectMulTo(1 - mfHitAlpha * mfHitZoomInAspectMul, 100);
	mpPlayer->FadeFOVMulTo(1 - mfHitAlpha * mfHitZoomInFOVMul, 100);

	mbSanityEffectUpdated = true;
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::UpdateEnemySeenEffect(float afTimeStep)
{
	if(mbEnemyIsSeen)
	{
		if(mfSeenEnemyCount <1)
		{
			mfSeenEnemyCount += afTimeStep * 0.3f;
			if(mfSeenEnemyCount>1)
			{
				mfSeenEnemyCount =1;
				gpBase->mpHintHandler->Add("EnemySeen", kTranslate("Hints", "EnemySeen"), 0);
			}
		}
		
		float fPulse = 0.5f + (sin(mfT*2.5f)*0.5f + 0.5f)*0.5f;
		
		//////////////////////////////////////////////////////////////////
		// THIS player's blur. cLuxPlayerSanity is per player and mbEnemyIsSeen
		// is measured from this player's own eyes -- but the radial blur behind
		// it is one shared post effect that cLuxMapHandler mirrors onto both
		// halves, so Player 2 walking into a monster put the tunnel vision on
		// Player 1's screen too, in an empty corridor.
		//
		// Released centrally, when the blur reaches zero -- see
		// cLuxEffect_RadialBlur::Update.
		gpBase->mpEffectHandler->SetRadialBlurOwner(mpPlayer);
		gpBase->mpEffectHandler->GetRadialBlur()->SetBlurStartDist(0.2f);
		gpBase->mpEffectHandler->GetRadialBlur()->FadeTo(0.12f * mfSeenEnemyCount*fPulse, 10.0f);
	}
	else
	{
		if(mfSeenEnemyCount > 0)
		{
			mfSeenEnemyCount -= afTimeStep * 0.15f;
			if(mfSeenEnemyCount<0)mfSeenEnemyCount =0;
		}
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::UpdateLosingSanity(float afTimeStep)
{
	if(mfSanityLostCount <= 0)
	{
		mfSanityDrainCount = 0;
		mfSanityHeartbeatCount =0;
		mfSanityDrainVolume -= afTimeStep*0.5f;
		if(mfSanityDrainVolume < 0) mfSanityDrainVolume =0;
		
		return;
	}
	
	float fSanity = mpPlayer->GetSanity();
	float fNormalizedSanity = fSanity / 100.0f;

	mbSanityLostIsUpdated = true;
	mfSanityLostCount -= afTimeStep;

	mfSanityDrainVolume += afTimeStep * 0.1f;
	if(mfSanityDrainVolume > 1) mfSanityDrainVolume =1;

	float mfSpeedMul = 1 + (1 - fNormalizedSanity) * 2.0f;
	
	mfSanityHeartbeatCount += afTimeStep * mfSpeedMul * 0.1f;
	if(mfSanityHeartbeatCount >= 1)
	{
		mfSanityHeartbeatCount =0;
		
		float fVol = (1.0f - fNormalizedSanity*0.5f) * mfSanityDrainVolume;

		if(mpPlayer->IsDead()==false)
			gpBase->mpHelpFuncs->PlayGuiSoundData(mpPlayer->ResolveVoiceSound("sanity_heartbeat"), eSoundEntryType_Gui, fVol);
	}
	
	mfSanityDrainCount += afTimeStep * mfSpeedMul * 0.33f;
	if(mfSanityDrainCount >= 1)
	{
		mfSanityDrainCount =0;
		tString sSoundFile="";
		if(fSanity > 75)
			sSoundFile = "sanity_drain_low";
		else if(fSanity > 50)
			sSoundFile = "sanity_drain_med"; 
		else
			sSoundFile = "sanity_drain_high";
		
		if(mpPlayer->IsDead()==false)
			gpBase->mpHelpFuncs->PlayGuiSoundData(mpPlayer->ResolveVoiceSound(sSoundFile), eSoundEntryType_Gui, mfSanityDrainVolume);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerSanity::UpdateLowSanity(float afTimeStep)
{
	if(mpPlayer->GetSanity() > mfSanityVeryLowLimit) return;
	
	if(mfShowHintTimer<=0)
	{
		mfShowHintTimer = 3.0f;
        gpBase->mpHintHandler->Add("SanityLow", kTranslate("Hints", "SanityLow"), 0);
	}
	else
	{
		mfShowHintTimer -= afTimeStep;
	}


	if(mbSanityLostIsUpdated==false)
	{
		gpBase->mpEffectHandler->GetImageTrail()->FadeTo(1.6f, 3);
		mbSanityEffectUpdated = true;
	}

	if(mfPantCount < 0)
	{
		mfPantCount = cMath::RandRectf(0.5f, 5.0f);

		//Play pant sound
		if(mpPlayer->IsDead()==false)
			gpBase->mpHelpFuncs->PlayGuiSoundData(mpPlayer->ResolveVoiceSound("sanity_pant"), eSoundEntryType_Gui);
	}
	else
	{
		mfPantCount -= afTimeStep;
	}
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER LANTERN
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// DEBUG GUN
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

// Everything the gun draws is built here from boxes, so it needs no art on disk.
// Same approach the coop avatar takes; kept local because those helpers are
// private to cLuxPlayerAvatar and this is the only other user.
static void GunAddVertex(iVertexBuffer *apVB, const cVector3f &avPos, const cVector3f &avNormal)
{
	apVB->AddVertexVec3f(eVertexBufferElement_Position, avPos);
	apVB->AddVertexVec3f(eVertexBufferElement_Normal, avNormal);
	apVB->AddVertexColor(eVertexBufferElement_Color0, cColor(1, 1));
	apVB->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(0, 0, 0));
}

//-----------------------------------------------------------------------

//The view model's placement, in camera space. Shared by the mesh that is built
//from it, the matrix that positions it and the muzzle the shot leaves from, so
//moving the gun on screen moves where its tracer starts with it.
static const cVector3f gvGunViewSize(0.075f, 0.11f, 0.34f);
static const cVector3f gvGunViewOffset(0.17f, -0.16f, -0.42f);
static const cVector3f gvArmViewSize(0.085f, 0.085f, 0.30f);
static const cVector3f gvArmViewOffset(0.19f, -0.20f, -0.16f);

//Thin. It is seen almost end-on from about half a metre away, so a couple of
//centimetres reads as a bar across the screen rather than as a tracer.
static const float gfGunTracerWidth = 0.010f;

//-----------------------------------------------------------------------

cLuxPlayerGun::cLuxPlayerGun(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerGun")
{
	mpGunTexture = NULL;	mpTeamTexture = NULL;
	mpGunMaterial = NULL;	mpTeamMaterial = NULL;
	mpGunMesh = NULL;		mpArmMesh = NULL;	mpTracerMesh = NULL;
	mpGunEntity = NULL;		mpArmEntity = NULL;	mpTracerEntity = NULL;
	mpFlashLight = NULL;
	mpEntityWorld = NULL;
	mpCrosshairGfx = NULL;

	// Blue for P1, red for P2 -- the same identity the avatars already use, so a
	// tracer tells you at a glance who fired it.
	cColor teamColor = mpPlayer->IsPlayer2() ? cColor(1.0f, 0.15f, 0.15f, 1.0f)
											 : cColor(0.2f, 0.45f, 1.0f, 1.0f);

	const tString sName = mpPlayer->IsPlayer2() ? "PlayerGunP2" : "PlayerGunP1";

	mpGunMaterial  = BuildFlatMaterial(sName + "_GunMat",  cColor(0.06f, 0.06f, 0.07f, 1.0f), &mpGunTexture);
	mpTeamMaterial = BuildFlatMaterial(sName + "_TeamMat", teamColor, &mpTeamTexture);

	// A body, and an arm behind it. Not a model -- a silhouette that reads as a
	// gun at the bottom of the screen and shows the recoil.
	mpGunMesh    = BuildBoxMesh(sName + "_Gun", gvGunViewSize, mpGunMaterial);
	mpArmMesh    = BuildBoxMesh(sName + "_Arm", gvArmViewSize, mpTeamMaterial);
	// Unit length in Z; the entity is scaled to reach the impact point.
	mpTracerMesh = BuildBoxMesh(sName + "_Tracer",
								cVector3f(gfGunTracerWidth, gfGunTracerWidth, 1.0f), mpTeamMaterial);

	// The team material goes to TWO submeshes (arm and tracer) and each releases
	// one reference when it dies. CreateCustomMaterial only gave us one, so
	// balance it -- same reason cLuxPlayerAvatar does.
	if(mpTeamMaterial) mpTeamMaterial->IncUserCount();

	// Our own reference on each mesh, so destroying the entities between maps
	// never deletes the meshes themselves.
	if(mpGunMesh)    mpGunMesh->IncUserCount();
	if(mpArmMesh)    mpArmMesh->IncUserCount();
	if(mpTracerMesh) mpTracerMesh->IncUserCount();

	Reset();
}

//-----------------------------------------------------------------------

cLuxPlayerGun::~cLuxPlayerGun()
{
	// World entities belong to the world and are dropped in DestroyWorldEntities.
	// Releasing our mesh references deletes the meshes, whose submeshes release
	// the materials, which release the textures.
	cMeshManager *pMeshManager = gpBase->mpEngine->GetResources()->GetMeshManager();

	if(mpGunMesh)    pMeshManager->Destroy(mpGunMesh);
	if(mpArmMesh)    pMeshManager->Destroy(mpArmMesh);
	if(mpTracerMesh) pMeshManager->Destroy(mpTracerMesh);

	if(mpCrosshairGfx) gpBase->mpEngine->GetGui()->DestroyGfx(mpCrosshairGfx);
}

//-----------------------------------------------------------------------

void cLuxPlayerGun::Reset()
{
	mbActive = false;
	mfCooldown = 0;
	mfRecoil = 0;
	mfTracerTime = 0;
}

//-----------------------------------------------------------------------

cMaterial* cLuxPlayerGun::BuildFlatMaterial(const tString &asName, const cColor &aColor, iTexture **apOutTexture)
{
	iLowLevelGraphics *pLowGfx = gpBase->mpEngine->GetGraphics()->GetLowLevel();
	cResources *pResources = gpBase->mpEngine->GetResources();

	iTexture *pTex = pLowGfx->CreateTexture(asName + "_Tex", eTextureType_2D, eTextureUsage_Normal);

	unsigned char vPixels[4*4*4];
	for(int i=0; i<4*4; ++i)
	{
		vPixels[i*4+0] = (unsigned char)(aColor.r * 255.0f);
		vPixels[i*4+1] = (unsigned char)(aColor.g * 255.0f);
		vPixels[i*4+2] = (unsigned char)(aColor.b * 255.0f);
		vPixels[i*4+3] = 255;
	}
	// z must be 1: the upload size is x*y*z*bpp, and z of 0 uploads nothing.
	pTex->CreateFromRawData(cVector3l(4, 4, 1), ePixelFormat_RGBA, vPixels);

	iMaterialType *pMatType = gpBase->mpEngine->GetGraphics()->GetMaterialType("soliddiffuse");
	cMaterial *pMat = pResources->GetMaterialManager()->CreateCustomMaterial(asName, pMatType);
	pMat->SetTexture(eMaterialTexture_Diffuse, pTex);
	pMat->Compile();

	if(apOutTexture) *apOutTexture = pTex;
	return pMat;
}

//-----------------------------------------------------------------------

cMesh* cLuxPlayerGun::BuildBoxMesh(const tString &asName, const cVector3f &avSize, cMaterial *apMaterial)
{
	iLowLevelGraphics *pLowGfx = gpBase->mpEngine->GetGraphics()->GetLowLevel();
	cResources *pResources = gpBase->mpEngine->GetResources();

	iVertexBuffer *pVB = pLowGfx->CreateVertexBuffer(
		eVertexBufferType_Hardware,
		eVertexBufferDrawType_Tri, eVertexBufferUsageType_Static,
		24, 36);

	pVB->CreateElementArray(eVertexBufferElement_Position, eVertexBufferElementFormat_Float, 4);
	pVB->CreateElementArray(eVertexBufferElement_Normal, eVertexBufferElementFormat_Float, 3);
	pVB->CreateElementArray(eVertexBufferElement_Color0, eVertexBufferElementFormat_Float, 4);
	pVB->CreateElementArray(eVertexBufferElement_Texture0, eVertexBufferElementFormat_Float, 3);

	const cVector3f vH = avSize * 0.5f;

	// Six faces with their own vertices, so each gets a flat normal and the box
	// lights like a box instead of a smoothed blob.
	const cVector3f vNormals[6] = {
		cVector3f( 0, 0, 1), cVector3f( 0, 0,-1),
		cVector3f( 1, 0, 0), cVector3f(-1, 0, 0),
		cVector3f( 0, 1, 0), cVector3f( 0,-1, 0) };

	for(int f=0; f<6; ++f)
	{
		const cVector3f &vN = vNormals[f];

		// Two axes across the face, picked so the winding stays outward.
		cVector3f vU, vV;
		if(vN.z != 0)      { vU = cVector3f(vH.x,0,0) * (vN.z > 0 ? 1.0f : -1.0f); vV = cVector3f(0,vH.y,0); }
		else if(vN.x != 0) { vU = cVector3f(0,0,vH.z) * (vN.x > 0 ? -1.0f : 1.0f); vV = cVector3f(0,vH.y,0); }
		else               { vU = cVector3f(vH.x,0,0); vV = cVector3f(0,0,vH.z) * (vN.y > 0 ? -1.0f : 1.0f); }

		const cVector3f vC(vN.x*vH.x, vN.y*vH.y, vN.z*vH.z);

		GunAddVertex(pVB, vC - vU - vV, vN);
		GunAddVertex(pVB, vC + vU - vV, vN);
		GunAddVertex(pVB, vC + vU + vV, vN);
		GunAddVertex(pVB, vC - vU + vV, vN);

		// The engine's cull-mode names are inverted at the backend: the default
		// eCullMode_CounterClockwise becomes glFrontFace(GL_CW), so a face is FRONT
		// facing when its vertex order's right-hand normal points AWAY from the
		// viewer. An outward face therefore has to be wound so that normal points
		// INWARD -- the opposite of the intuitive order, which is what this had and
		// why the box rendered inside-out. cLuxPlayerAvatar::CreateSphereVB winds
		// the same way for the same reason.
		const int lBase = f*4;
		pVB->AddIndex(lBase+0); pVB->AddIndex(lBase+2); pVB->AddIndex(lBase+1);
		pVB->AddIndex(lBase+0); pVB->AddIndex(lBase+3); pVB->AddIndex(lBase+2);
	}

	pVB->Compile(eVertexCompileFlag_CreateTangents);

	cMesh *pMesh = hplNew( cMesh, (asName, _W(""), pResources->GetMaterialManager(), pResources->GetAnimationManager()) );
	cSubMesh *pSubMesh = pMesh->CreateSubMesh("Main");
	pSubMesh->SetMaterial(apMaterial);
	pSubMesh->SetVertexBuffer(pVB);

	return pMesh;
}

//-----------------------------------------------------------------------

void cLuxPlayerGun::CreateWorldEntities(cLuxMap *apMap)
{
	cWorld *pWorld = apMap->GetWorld();

	// If the cached entities belong to a DIFFERENT world, that world already took
	// them down with it -- quit-to-menu calls DestroyMap() directly and never
	// reaches DestroyWorldEntities. Drop the stale pointers WITHOUT touching them
	// (already freed). Same use-after-free cLuxPlayerAvatar hit.
	if(mpEntityWorld != pWorld)
	{
		mpGunEntity = NULL;
		mpArmEntity = NULL;
		mpTracerEntity = NULL;
		mpFlashLight = NULL;
		mpEntityWorld = pWorld;
	}

	//Already built for this world.
	if(mpGunEntity) return;

	const tString sName = mpPlayer->IsPlayer2() ? "PlayerGunP2" : "PlayerGunP1";

	//Each entity's destructor releases one mesh reference.
	if(mpGunMesh)    mpGunMesh->IncUserCount();
	if(mpArmMesh)    mpArmMesh->IncUserCount();
	if(mpTracerMesh) mpTracerMesh->IncUserCount();

	mpGunEntity    = pWorld->CreateMeshEntity(sName + "_ViewGun", mpGunMesh, false);
	mpArmEntity    = pWorld->CreateMeshEntity(sName + "_ViewArm", mpArmMesh, false);
	mpTracerEntity = pWorld->CreateMeshEntity(sName + "_Tracer",  mpTracerMesh, false);

	cMeshEntity *vEnts[3] = { mpGunEntity, mpArmEntity, mpTracerEntity };
	for(int i=0; i<3; ++i)
	{
		if(vEnts[i]==NULL) continue;
		vEnts[i]->SetVisible(false);
		// A view model that casts shadows or shows up in reflections gives the
		// whole trick away.
		vEnts[i]->SetRenderFlagBit(eRenderableFlag_ShadowCaster, false);
		vEnts[i]->SetRenderFlagBit(eRenderableFlag_VisibleInReflection, false);
	}

	// One light for the whole map, faded per shot rather than created and thrown
	// away on every trigger pull. cLuxPlayerInDarkness uses FadeTo the same way.
	mpFlashLight = pWorld->CreateLightPoint(sName + "_MuzzleFlash", "", false);
	if(mpFlashLight)
	{
		mpFlashLight->SetDiffuseColor(cColor(0,0));
		mpFlashLight->SetRadius(4.5f);
		mpFlashLight->SetCastShadows(false);
		mpFlashLight->SetIsSaved(false);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerGun::DestroyWorldEntities(cLuxMap *apMap)
{
	cWorld *pWorld = apMap->GetWorld();

	// Never call DestroyMeshEntity on a world that does not own them.
	if(mpEntityWorld != pWorld)
	{
		mpGunEntity = NULL;
		mpArmEntity = NULL;
		mpTracerEntity = NULL;
		mpFlashLight = NULL;
		mpEntityWorld = NULL;
		return;
	}

	if(mpGunEntity)    pWorld->DestroyMeshEntity(mpGunEntity);
	if(mpArmEntity)    pWorld->DestroyMeshEntity(mpArmEntity);
	if(mpTracerEntity) pWorld->DestroyMeshEntity(mpTracerEntity);
	if(mpFlashLight)   pWorld->DestroyLight(mpFlashLight);

	mpGunEntity = NULL;
	mpArmEntity = NULL;
	mpTracerEntity = NULL;
	mpFlashLight = NULL;
	mpEntityWorld = NULL;
}

//-----------------------------------------------------------------------

void cLuxPlayerGun::SetActive(bool abX)
{
	if(::ImGuiDebugMenu::GetDebugGuns()==false) abX = false;
	if(mbActive == abX) return;

	mbActive = abX;

	// One thing in your hands at a time. Done here rather than at the input site
	// so the script and item paths that raise the lantern cannot desync from it.
	if(mbActive && mpPlayer->GetHelperLantern() && mpPlayer->GetHelperLantern()->IsActive())
		mpPlayer->GetHelperLantern()->SetActive(false, true);

	if(mbActive == false)
	{
		mfRecoil = 0;
		mfTracerTime = 0;
	}

	if(mpGunEntity) mpGunEntity->SetVisible(mbActive);
	if(mpArmEntity) mpArmEntity->SetVisible(mbActive);
	if(mpTracerEntity && mbActive==false) mpTracerEntity->SetVisible(false);
}

//-----------------------------------------------------------------------

void cLuxPlayerGun::SetViewModelVisibleForCamera(bool abX)
{
	//CRASH GUARD. Same window cLuxPlayerAvatar::SetEntitiesVisible guards, for the
	//same reason: this runs from the viewport PRE-DRAW callback, so it can fire
	//between a world being destroyed and this helper being told about it. Quit to
	//menu calls DestroyMap() directly and never reaches DestroyWorldEntities, so
	//the entity pointers refer to freed memory and even reading IsVisible() off
	//one is an access violation.
	//
	//Three cheap tells that we are inside that window:
	//  - the player has no character body, i.e. its world entities are gone
	//  - we never had a world, or already gave it up
	//  - our cached world is no longer registered with the scene
	if(mpPlayer == NULL || mpPlayer->GetCharacterBody() == NULL) return;
	if(mpEntityWorld == NULL) return;
	if(gpBase->mpEngine->GetScene()->WorldExists(mpEntityWorld) == false) return;

	// Called per viewport: a first-person model must not appear in the other
	// player's half of the screen.
	const bool bShow = abX && mbActive;

	if(mpGunEntity && mpGunEntity->IsVisible() != bShow) mpGunEntity->SetVisible(bShow);
	if(mpArmEntity && mpArmEntity->IsVisible() != bShow) mpArmEntity->SetVisible(bShow);
}

//-----------------------------------------------------------------------

//----------------------------------------------------------------------------
// A ray that can actually hit a monster.
//
// cLuxMapHelper::GetClosestEntity cannot: its callback reads the entity pointer
// off the physics body, and a character body keeps that pointer on its
// cCharacterBody instead -- so GetUserData() is NULL for every enemy, and the
// callback's else branch drops anything with IsCharacter() set. Right for
// finding something to interact with, wrong for a bullet, which should stop at
// the first solid thing in front of it whatever that thing is.
//----------------------------------------------------------------------------
class cLuxGunRayCallback : public iPhysicsRayCallback
{
public:
	void Reset()
	{
		mfClosestDist = -1;
		mpClosestBody = NULL;
	}

	bool BeforeIntersect(iPhysicsBody *apBody)
	{
		if(apBody->GetCollide()==false) return false;

		//Skip BOTH players. Yours because the ray starts inside it, theirs because
		//friendly fire is off -- and a shot that silently stopped dead in your
		//team-mate while the monster behind them walked on would be worse than one
		//that goes through.
		if(apBody->IsCharacter())
		{
			if(gpBase->mpPlayer && gpBase->mpPlayer->GetCharacterBody() &&
				apBody == gpBase->mpPlayer->GetCharacterBody()->GetCurrentBody()) return false;

			if(gpBase->mpPlayer2 && gpBase->mpPlayer2->GetCharacterBody() &&
				apBody == gpBase->mpPlayer2->GetCharacterBody()->GetCurrentBody()) return false;
		}

		return true;
	}

	bool OnIntersect(iPhysicsBody *apBody, cPhysicsRayParams *apParams)
	{
		if(mfClosestDist < 0 || apParams->mfDist < mfClosestDist)
		{
			mfClosestDist = apParams->mfDist;
			mpClosestBody = apBody;
		}
		return true;
	}

	float mfClosestDist;
	iPhysicsBody *mpClosestBody;
};

static cLuxGunRayCallback gGunRayCallback;

//----------------------------------------------------------------------------

//Reticle geometry, in HUD virtual units.
static const float gfCrosshairGap = 5.0f;		//clear space around the middle
static const float gfCrosshairTick = 9.0f;		//length of each arm
static const float gfCrosshairThick = 2.0f;
static const float gfCrosshairDot = 2.0f;

void cLuxPlayerGun::DrawCrosshair(cGuiSet *apSet, const cVector2f &avSetSize)
{
	if(apSet==NULL) return;
	if(mbActive==false) return;
	if(::ImGuiDebugMenu::GetDebugGuns()==false) return;

	//One white rect, tinted per piece by DrawGfx. Built here rather than in the
	//constructor because the gui does not exist yet when the helpers are made.
	if(mpCrosshairGfx==NULL)
		mpCrosshairGfx = gpBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1,1), eGuiMaterial_Alpha);
	if(mpCrosshairGfx==NULL) return;

	const cColor colTeam = mpPlayer->IsPlayer2() ? cColor(1.0f, 0.30f, 0.30f, 1.0f)
												 : cColor(0.45f, 0.65f, 1.0f, 1.0f);
	const cColor colPlate(0, 0, 0, 0.75f);

	const float fCx = avSetSize.x * 0.5f;
	const float fCy = avSetSize.y * 0.5f;
	const float fHalf = gfCrosshairThick * 0.5f;
	const float fDotHalf = gfCrosshairDot * 0.5f;

	//Four arms and a dot. The GAP is the point: a solid cross hides the thing you
	//are aiming at, which in a game this dark is the whole problem.
	cVector2f vPos[5];
	cVector2f vSize[5];

	vPos[0] = cVector2f(fCx - gfCrosshairGap - gfCrosshairTick, fCy - fHalf);
	vSize[0] = cVector2f(gfCrosshairTick, gfCrosshairThick);

	vPos[1] = cVector2f(fCx + gfCrosshairGap, fCy - fHalf);
	vSize[1] = vSize[0];

	vPos[2] = cVector2f(fCx - fHalf, fCy - gfCrosshairGap - gfCrosshairTick);
	vSize[2] = cVector2f(gfCrosshairThick, gfCrosshairTick);

	vPos[3] = cVector2f(fCx - fHalf, fCy + gfCrosshairGap);
	vSize[3] = vSize[2];

	vPos[4] = cVector2f(fCx - fDotHalf, fCy - fDotHalf);
	vSize[4] = cVector2f(gfCrosshairDot, gfCrosshairDot);

	//Dark plate first, one unit proud on every side. Amnesia is dark, but the
	//lantern and the candles are not, and a two-unit line in the player's colour
	//disappears completely against a lit wall.
	for(int i=0; i<5; ++i)
	{
		apSet->DrawGfx(mpCrosshairGfx,
					   cVector3f(vPos[i].x - 1.0f, vPos[i].y - 1.0f, 2.0f),
					   cVector2f(vSize[i].x + 2.0f, vSize[i].y + 2.0f), colPlate);
	}

	for(int i=0; i<5; ++i)
	{
		apSet->DrawGfx(mpCrosshairGfx, cVector3f(vPos[i].x, vPos[i].y, 2.1f), vSize[i], colTeam);
	}
}

//-----------------------------------------------------------------------

cVector3f cLuxPlayerGun::GetMuzzlePosition()
{
	cCamera *pCam = mpPlayer->GetCamera();
	if(pCam==NULL) return cVector3f(0);

	//Same basis Update() places the view model with, so the barrel tip this
	//returns is the barrel tip you can see. Recoil is deliberately left out:
	//Fire() sets the kick to full in the same instant, and the shot should leave
	//from where the gun was aimed, not from where it has just been thrown.
	cVector3f vCamRotation(pCam->GetPitch(), pCam->GetYaw(), pCam->GetRoll());
	cMatrixf mtxCam = cMath::MatrixRotate(vCamRotation, eEulerRotationOrder_XYZ);
	mtxCam.SetTranslation(pCam->GetPosition());

	cVector3f vLocalMuzzle = gvGunViewOffset;
	vLocalMuzzle.z -= gvGunViewSize.z * 0.5f;	//-Z is forward

	return cMath::MatrixMul(mtxCam, vLocalMuzzle);
}

//-----------------------------------------------------------------------

void cLuxPlayerGun::Fire()
{
	if(mbActive==false) return;
	if(mfCooldown > 0) return;
	if(::ImGuiDebugMenu::GetDebugGuns()==false) return;

	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL) return;

	cCamera *pCam = mpPlayer->GetCamera();
	if(pCam==NULL) return;

	mfCooldown = 0.28f;
	mfRecoil = 1.0f;

	// GetInteractionForward, not the camera forward: in split screen the camera
	// each player aims with is not the one the renderer last touched.
	const cVector3f vStart = pCam->GetPosition();
	const cVector3f vDir = mpPlayer->GetInteractionForward();
	const float fRange = 100.0f;

	////////////////////////////////
	// Where does it land
	float fDist = fRange;
	iPhysicsBody *pHitBody = NULL;

	iPhysicsWorld *pPhysicsWorld = pMap->GetPhysicsWorld();
	if(pPhysicsWorld)
	{
		gGunRayCallback.Reset();
		//abCalcDist, so OnIntersect can pick the nearest hit.
		pPhysicsWorld->CastRay(&gGunRayCallback, vStart, vStart + vDir*fRange,
							   true, false, false, true);

		pHitBody = gGunRayCallback.mpClosestBody;
		if(pHitBody) fDist = gGunRayCallback.mfClosestDist;
	}

	const bool bHitSomething = (pHitBody != NULL);
	if(bHitSomething==false) fDist = fRange;

	const cVector3f vHitPos = vStart + vDir * fDist;

	//Everything you can SEE of the shot starts at the barrel, not at the eye.
	const cVector3f vMuzzle = GetMuzzlePosition();

	////////////////////////////////
	// Muzzle flash
	//
	// A real world light, so both players see the same flash in the same place --
	// only the view model differs between the two views.
	if(mpFlashLight)
	{
		mpFlashLight->SetPosition(vMuzzle);
		mpFlashLight->SetDiffuseColor(cColor(1.0f, 0.85f, 0.5f, 1.0f));
		mpFlashLight->SetRadius(4.5f);
		mpFlashLight->FadeTo(cColor(0,0), 4.5f, 0.09f);
	}

	////////////////////////////////
	// Tracer
	//
	// Barrel to impact, NOT eye to impact. Starting it at the camera put the near
	// end of the beam on the near clip plane, so you were looking down the inside
	// of it -- it filled the screen and looked like it came out of your face.
	if(mpTracerEntity)
	{
		const cVector3f vBeam = vHitPos - vMuzzle;
		const float fBeamLength = vBeam.Length();

		//Point blank. Nothing worth drawing, and no direction to build a basis from.
		if(fBeamLength < 0.05f)
		{
			mpTracerEntity->SetVisible(false);
			mfTracerTime = 0;
		}
		else
		{
			const cVector3f vMid = vMuzzle + vBeam * 0.5f;

			// Build the basis by hand, the way cLuxPlayerAvatar::SetSegmentMatrix does
			// for its limbs. There is no cMath helper that rotates one unit vector onto
			// another, and the length is baked into the Z axis so the unit-long mesh
			// spans exactly muzzle to impact.
			cVector3f vZ = cMath::Vector3Normalize(vBeam);
			cVector3f vRef = (fabsf(vZ.y) < 0.95f) ? cVector3f(0, 1, 0) : cVector3f(1, 0, 0);
			cVector3f vX = cMath::Vector3Cross(vRef, vZ);
			vX.Normalize();
			cVector3f vY = cMath::Vector3Cross(vZ, vX);
			const cVector3f &vZs = vBeam;

			cMatrixf mtxTracer(vX.x, vY.x, vZs.x, vMid.x,
							   vX.y, vY.y, vZs.y, vMid.y,
							   vX.z, vY.z, vZs.z, vMid.z,
							   0, 0, 0, 1);

			mpTracerEntity->SetMatrix(mtxTracer);
			mpTracerEntity->SetVisible(true);
			mfTracerTime = 0.06f;
		}
	}

	////////////////////////////////
	// Sound
	//
	// Play3D takes a raw file name. CreateSoundEntity would need a .snt authored
	// next to the wav, and the point of a debug gun is that it needs no assets.
	cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	if(pSoundHandler)
		pSoundHandler->Play3D("player_gun.wav", false, 1.0f, vStart, 1.0f, 30.0f, eSoundEntryType_World);

	////////////////////////////////
	// The hit
	//
	// ShapeDamage with a small sphere at the impact point, rather than applying
	// damage and impulse by hand: it already does the damage, the mass-scaled
	// impulse, the enemy hit sound and particle, AND the surface-appropriate
	// impact effect off cSurfaceData -- the same path the melee code takes. One
	// call, and it cannot drift away from how the rest of the game hits things.
	if(bHitSomething)
	{
		if(pPhysicsWorld)
		{
			iCollideShape *pShape = pPhysicsWorld->CreateSphereShape(cVector3f(0.12f), NULL);
			if(pShape)
			{
				const float fDamage = ::ImGuiDebugMenu::GetGunDamage();
				const float fForce = ::ImGuiDebugMenu::GetGunImpactForce();

				//Suppress the enemy state resistance for this one call. Without it a
				//hunting monster takes a fifth of the number in the menu, and the gun
				//feels like it is firing blanks at exactly the moment you need it.
				iLuxEnemy::SetIgnoreDamageResistance(true);

				gpBase->mpMapHelper->ShapeDamage(
					pShape, cMath::MatrixTranslate(vHitPos), vStart,
					fDamage, fDamage,
					fForce, fForce,
					// Strength 100 clears every mlToughness tier -- a bullet is not
					// one of the game's weapon grades.
					100, 8.0f,
					eLuxDamageType_BloodSplat, eLuxWeaponHitType_Bullet,
					true, false, true, false);

				iLuxEnemy::SetIgnoreDamageResistance(false);

				pPhysicsWorld->DestroyShape(pShape);
			}
		}
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerGun::Update(float afTimeStep)
{
	// Switching the feature off mid-game must put the gun away, or it stays
	// floating with no way to dismiss it.
	if(::ImGuiDebugMenu::GetDebugGuns()==false && mbActive)
		SetActive(false);

	if(mfCooldown > 0) mfCooldown -= afTimeStep;

	if(mfTracerTime > 0)
	{
		mfTracerTime -= afTimeStep;
		if(mfTracerTime <= 0 && mpTracerEntity) mpTracerEntity->SetVisible(false);
	}

	// Recoil springs back on its own. Fast enough to be settled before the
	// cooldown lets the next shot through.
	if(mfRecoil > 0)
	{
		mfRecoil -= afTimeStep * 6.0f;
		if(mfRecoil < 0) mfRecoil = 0;
	}

	if(mbActive==false) return;
	if(mpGunEntity==NULL || mpArmEntity==NULL) return;

	//Same stale-world window as SetViewModelVisibleForCamera -- see the note there.
	//Reached from the updater rather than the render callback, but the pointers are
	//the same ones.
	if(mpEntityWorld == NULL) return;
	if(gpBase->mpEngine->GetScene()->WorldExists(mpEntityWorld) == false) return;

	cCamera *pCam = mpPlayer->GetCamera();
	if(pCam==NULL) return;

	////////////////////////////////
	// View model
	//
	// Camera-relative rather than attached to a hand bone: bone attachment needs
	// an iLuxHandObject and a .ho on disk, and this builds itself at runtime.
	// Same matrix idiom cLuxPlayerLantern uses for its light.
	cVector3f vCamRotation(pCam->GetPitch(), pCam->GetYaw(), pCam->GetRoll());
	cMatrixf mtxCam = cMath::MatrixRotate(vCamRotation, eEulerRotationOrder_XYZ);
	mtxCam.SetTranslation(pCam->GetPosition());

	// Recoil rides the local offset: back and up, which reads as a kick without
	// anything to animate.
	const float fKick = mfRecoil * mfRecoil;

	cVector3f vGunOffset = gvGunViewOffset + cVector3f(0, fKick*0.03f, fKick*0.10f);
	cVector3f vArmOffset = gvArmViewOffset + cVector3f(0, fKick*0.02f, fKick*0.08f);

	cMatrixf mtxGunLocal = cMath::MatrixRotate(cVector3f(fKick * -0.35f, 0, 0), eEulerRotationOrder_XYZ);
	mtxGunLocal.SetTranslation(vGunOffset);

	cMatrixf mtxArmLocal = cMath::MatrixTranslate(vArmOffset);

	mpGunEntity->SetMatrix(cMath::MatrixMul(mtxCam, mtxGunLocal));
	mpArmEntity->SetMatrix(cMath::MatrixMul(mtxCam, mtxArmLocal));
}

//-----------------------------------------------------------------------

cLuxPlayerLantern::cLuxPlayerLantern(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerLantern")
{
	// The whole lantern identity comes from the player's own config
	// (P2 = Justine's lantern, when installed).
	cConfigFile *pCfg = apPlayer->GetPlayerCfg();
	mDefaultColor = pCfg->GetColor("Player_Lantern","Color",cColor(0));
	mfRadius = pCfg->GetFloat("Player_Lantern","Radius",0);
	msGobo = pCfg->GetString("Player_Lantern","Gobo","");
	mvLocalOffset = pCfg->GetVector3f("Player_Lantern","LocalOffset",0);
	mbCastShadows = pCfg->GetBool("Player_Lantern","CastShadows",false);
	mfLowerOilSpeed = pCfg->GetFloat("Player_Lantern","LowerOilSpeed",0);
	mfFadeLightOilAmount = pCfg->GetFloat("Player_Lantern","FadeLightOilAmount",0);

	msOutOfOilSound = pCfg->GetString("Player_Lantern","OutOfOilSound","");
	msDisabledSound = pCfg->GetString("Player_Lantern","DisabledSound","");
	msTurnOnSound = pCfg->GetString("Player_Lantern","TurnOnSound","");
	msTurnOffSound = pCfg->GetString("Player_Lantern","TurnOffSound","");

	Reset();
}

cLuxPlayerLantern::~cLuxPlayerLantern()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerLantern::OnStart()
{

}
void cLuxPlayerLantern::Reset()
{
	mbDisabled = false;
	mbActive = false;
	mpLight = NULL;
	mfAlpha =0;
}

//-----------------------------------------------------------------------


void cLuxPlayerLantern::Update(float afTimeStep)
{
	if(mbActive ==false && mfAlpha <=0)
	{
		return;
	}
	
	////////////////////////////
	// Fade in light
	if(mbActive)
	{
		mfAlpha += afTimeStep;
		if(mfAlpha > 1.0f) mfAlpha =1;
	}
	else if(mfAlpha > 0)
	{
		mfAlpha -= afTimeStep*2.0f;
		if(mfAlpha < 0) mfAlpha =0;
	}

	cColor lightColor = mDefaultColor;
	float fOil = mpPlayer->GetLampOil();
	if(fOil < mfFadeLightOilAmount)
	{
		lightColor =  mDefaultColor * (fOil / mfFadeLightOilAmount);
	}
	mpLight->SetDiffuseColor(lightColor * mfAlpha);


	////////////////////////////
	// Lower oil
	if(mbActive && ::ImGuiDebugMenu::GetFreeUseLantern()==false &&
		gpBase->mpEffectHandler->GetEmotionFlash()->IsActive()==false)
	{
		float fOil = mpPlayer->GetLampOil();
		fOil -= mfLowerOilSpeed *afTimeStep;
		if(fOil <=0)
		{
			fOil = 0;
			gpBase->mpHelpFuncs->PlayGuiSoundData(msOutOfOilSound, eSoundEntryType_Gui);
			SetActive(false, true);
		}
		mpPlayer->SetLampOil(fOil);
	}

	////////////////////////////
	// Update light matrix
	if(mpLight)
	{
		cCamera *pCam = mpPlayer->GetCamera();
		cVector3f vCamRotation( pCam->GetPitch(), pCam->GetYaw(), pCam->GetRoll());
		cMatrixf mtxCamTransform = cMath::MatrixRotate(vCamRotation, eEulerRotationOrder_XYZ);
		mtxCamTransform.SetTranslation(pCam->GetPosition());

		cMatrixf mtxLocalLight = cMath::MatrixTranslate(mvLocalOffset);

		mpLight->SetMatrix(cMath::MatrixMul(mtxCamTransform, mtxLocalLight));
	}
}

//-----------------------------------------------------------------------
void cLuxPlayerLantern::OnMapEnter(cLuxMap *apMap)
{
	
}

void cLuxPlayerLantern::OnMapLeave(cLuxMap *apMap)
{

}

//-----------------------------------------------------------------------

void cLuxPlayerLantern::CreateWorldEntities(cLuxMap *apMap)
{
	cWorld *pWorld = apMap->GetWorld();

	cCamera *pCam = mpPlayer->GetCamera();

	// Unique light name per player so P2 doesn't collide with P1's lantern light
	tString sLightName = mpPlayer->IsPlayer2() ? "PlayerLantern_P2" : "PlayerLantern";
	mpLight = pWorld->CreateLightPoint(sLightName,msGobo,false);
	mpLight->SetDiffuseColor(cColor(0,0));
	mpLight->SetRadius(mfRadius);
	
	mpLight->SetIsSaved(false);

	if(mbActive)
	{
		mbActive=false;
		SetActive(true, false);
	}
}

void cLuxPlayerLantern::DestroyWorldEntities(cLuxMap *apMap)
{
	if(mpLight) apMap->GetWorld()->DestroyLight(mpLight);
	mpLight = NULL;
}

//-----------------------------------------------------------------------

void cLuxPlayerLantern::SetActive(bool abX, bool abUseEffects, bool abCheckForOilAndItems, bool abCheckIfAllowed)
{
	if(mbActive == abX) return;

	/////////////////
	// Check so allowed
	if(abCheckIfAllowed && mpPlayer->GetCurrentStateData()->AllowLantern()==false)
	{
		return;
	}

	/////////////////
	// Check so THIS player has the lantern item (the inventories are per
	// player — swap the global so the check looks at the owner's items).
	if(abCheckForOilAndItems)
	{
		// Ask about THIS player explicitly.
		//
		// This used to swap gpBase->mpPlayer to P2 and call HasItemOfType(), on the
		// assumption that the swap redirects the inventory. It does not:
		// HasItemOfType() -> GetActiveItems() picks its list from the inventory's
		// OWN mpActivePlayer (whoever last opened a bag), which the swap never
		// touches. So the check silently answered for the wrong player -- P2 could
		// be holding the lantern and still be told "you have no lantern", by X or
		// by the inventory, whatever the Lantern Per Player setting was.
		bool bHasLantern = gpBase->mpInventory->PlayerHasItemOfType(mpPlayer, eLuxItemType_Lantern);
		if(!bHasLantern && gpBase->mpMapHandler->GetCoopMode() && ImGuiDebugMenu::GetLanternPerPlayer())
			bHasLantern = gpBase->mpInventory->AnyPlayerHasItemOfType(eLuxItemType_Lantern);

		if(bHasLantern==false)
		{
			gpBase->mpHintHandler->Add("LanternNoItem", kTranslate("Hints", "LanternNoItem"), 0);
			return;
		}
	}

	/////////////////
	// Check if disabled
	if(abX && mbDisabled)
	{
		return;
	}
	
	/////////////////
	// Check if there is enough oil
	//
	// Half of Free Use Lantern. Skipping only the drain in Update leaves an
	// already-empty lantern refusing to light, which reads as the cheat not
	// working at all.
    if(abCheckForOilAndItems && abX && mpPlayer->GetLampOil() <=0 &&
		::ImGuiDebugMenu::GetFreeUseLantern()==false)
	{
		if(abUseEffects)
		{
			gpBase->mpHintHandler->Add("LanternNoOil", kTranslate("Hints", "LanternNoOil"), 0);
			gpBase->mpHelpFuncs->PlayGuiSoundData(msOutOfOilSound, eSoundEntryType_Gui);
		}
		return;
	}


	/////////////////
	// Turn on / off
	mbActive = abX;
	if(mbActive)
	{
		if(abUseEffects) gpBase->mpHelpFuncs->PlayGuiSoundData(msTurnOnSound, eSoundEntryType_Gui);

		// For P2, set alpha immediately since P2's update may not fade correctly
		if(mpPlayer->IsPlayer2())
		{
			mfAlpha = 1.0f;
			if(mpLight)
			{
				mpLight->SetDiffuseColor(mDefaultColor);
			}
		}
	}
	else
	{
		if(abUseEffects) gpBase->mpHelpFuncs->PlayGuiSoundData(msTurnOffSound, eSoundEntryType_Gui);
	}

	/////////////////
	// Hand (P2 has no hands object — skip hand model activation)
	if(mpPlayer->GetHands())
	{
		if(mbActive)
			mpPlayer->GetHands()->SetActiveHandObject("lantern");
		else
			mpPlayer->GetHands()->SetActiveHandObject("");
	}


	/////////////////
	// Callback
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap->GetLanternLitCallback()!="")
	{
		pMap->RunScript(pMap->GetLanternLitCallback()+"(" + (mbActive ? "true" : "false") + ")" );
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerLantern::SetDisabled(bool abX)
{
	mbDisabled = abX;

	if(mbDisabled)
		SetActive(false, true);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER DEATH
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerDeath::cLuxPlayerDeath(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "PlayerDeath")
{
	cGui *pGui = gpBase->mpEngine->GetGui();

	mpWhiteModGfx = pGui->CreateGfxFilledRect(cColor(1,1),eGuiMaterial_Modulative);
	
	mfHeightAddGoal = gpBase->mpGameCfg->GetFloat("Player_General","Death_HeightAdd",0);
	mfHeightAddGoalCrouch = gpBase->mpGameCfg->GetFloat("Player_General","Death_HeightAddCrouch",0);
	mfFadeOutTime = gpBase->mpGameCfg->GetFloat("Player_General","Death_FadeTime",1000);
	
	if(mpPlayer->UsePermaDeath()) mfFadeOutTime /= 2.0f;

	mfMaxSanityGain = gpBase->mpGameCfg->GetFloat("Player_General","Death_MaxSanityGain",0);
	mfMaxHealthGain = gpBase->mpGameCfg->GetFloat("Player_General","Death_MaxHealthGain",0);
	mfMaxOilGain = gpBase->mpGameCfg->GetFloat("Player_General","Death_MaxOilGain",0);

	mfMinSanityGain = gpBase->mpGameCfg->GetFloat("Player_General","Death_MinSanityGain",0);
	mfMinHealthGain = gpBase->mpGameCfg->GetFloat("Player_General","Death_MinHealthGain",0);
	mfMinOilGain = gpBase->mpGameCfg->GetFloat("Player_General","Death_MinOilGain",0);

	mfHeightAddSpeed = gpBase->mpGameCfg->GetFloat("Player_General","Death_HeightAddSpeed",0);
	mfRollSpeed = gpBase->mpGameCfg->GetFloat("Player_General","Death_RollSpeed",0);

	if(mpPlayer->UsePermaDeath()) mfHeightAddSpeed *= 1.2f;
	if(mpPlayer->UsePermaDeath()) mfRollSpeed *= 1.2f;

	// Voice sounds come from the player's own config (P2 = Justine's, when installed)
	msStartSound = mpPlayer->GetPlayerCfg()->GetString("Player_General","Death_StartSound", "");
	msAwakenSound = mpPlayer->GetPlayerCfg()->GetString("Player_General","Death_AwakenSound", "");

	mpFont = NULL;

	mFlashOscill.SetUp(0,1,0,0.5,0.5);

	mbToMainMenu = false;
}

cLuxPlayerDeath::~cLuxPlayerDeath()
{
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::LoadFonts()
{
	mpFont = LoadFont("game_default.fnt");
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::LoadUserConfig()
{
	mbShowHint = gpBase->mpUserConfig->GetBool("Game", "ShowDeathHints", true);
}

void cLuxPlayerDeath::SaveUserConfig()
{
	gpBase->mpUserConfig->SetBool("Game", "ShowDeathHints", mbShowHint);
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::Reset()
{
	mfHeightAdd =0;
	mfRoll =0;
	mfTextAlpha1 =0;
	mfTextAlpha2 =0;
	mfWhiteCount =0;

	mpVoiceEntry = NULL;

	mbSkipStartSound = false;

	msCurrentHintText = _W("");

	msHintCat = "";
	msHintEntry = "";

	mfFadeAlpha =0;
	mfTextOnScreenCount =0;

	mlState =0;

	mfT =0;

	mbActive = false;
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::Start()
{
	if(mbActive) return;

	/////////////////////////////////////
	// Coop: death is SHARED — one player going down is game over for both.
	// P2's death redirects into P1's death helper, which runs the single
	// authoritative sequence (fade, hint, checkpoint reset) for both players.
	if(mpPlayer->IsPlayer2())
	{
		////////////////////////////////////////////////////////////////////
		// THE CUSTOM-STORY P2-DEATH FREEZE.
		//
		// gpBase->mpPlayer is NOT always Player 1: collide / interact / lookat /
		// use-item / timer script callbacks run with it SWAPPED to the player who
		// triggered them (CoopBeginActAs) -- which is exactly how a custom story's
		// scripted kill area fires GivePlayerDamage at P2. Reading it here then
		// gave pP1 == P2; P2 had just died, so IsDead() was true, and
		// pP1->GetHelperDeath()->Start() re-entered THIS function -- whose
		// mbActive guard never arms, because this branch returns before
		// mbActive = true. Infinite recursion, stack overflow, and the game
		// "freezes" with nothing in the log.
		//
		// Resolve the REAL P1: if the global currently names P2, the genuine P1 is
		// whoever the swap stashed away. If no real P1 can be found (cannot
		// happen, but the cost of the check is nothing), do nothing rather than
		// recurse.
		cLuxPlayer *pP1 = gpBase->mpPlayer;
		if(pP1 && pP1->IsPlayer2()) pP1 = cLuxPlayer::CoopGetSwappedOutPlayer();

		if(pP1 && pP1->IsPlayer2()==false)
		{
			//SetHealth would be the tidier call, and it is -- right up until P1 is
			//ALREADY at or below zero, because it opens with
			//`if(mfHealth <=0 && afX <= 0) return;` and then does nothing whatsoever:
			//no death sequence, no fade, no checkpoint reload. P2 is left
			//dead-but-not-dying, and since IsDead() gates their entire input path they
			//are frozen for good -- no movement, no camera, not even their inventory --
			//while the game carries on around them.
			//
			//Start() is re-entrant-safe on its own (`if(mbActive) return;`), so going
			//straight to the helper cannot disturb a sequence already running.
			if(pP1->IsDead()==false)	pP1->SetHealth(0);
			else						pP1->GetHelperDeath()->Start();
		}
		return;
	}

	mbActive = true;

	/////////////////////////////////////
	// Coop: take the other player down with us (their input freezes while
	// dead; both are revived together in ResetGame).
	//
	// The co-victim gets the FULL death presentation, not just frozen input:
	// same red damage flash as a real hit (only if they were not already dead
	// -- a player killed by actual damage flashed in GiveDamage), same state
	// reset, and the fall/fade/hint/white-flash are mirrored onto their camera
	// and hud set in Update()/OnDraw() below.
	cLuxPlayer *pP2Shared = gpBase->mpPlayer2;
	if(gpBase->mpMapHandler->GetCoopMode() && pP2Shared)
	{
		if(pP2Shared->IsDead()==false)
		{
			pP2Shared->SetHealth(0);
			if(pP2Shared->GetHelperHudEffect())
				pP2Shared->GetHelperHudEffect()->Flash(cColor(0.6f,0,0, 0.5f),eGuiMaterial_Alpha,0,0.25f);
		}
		pP2Shared->ChangeState(eLuxPlayerState_Normal);
		pP2Shared->ChangeMoveState(eLuxMoveState_Normal);
		pP2Shared->GetHelperLantern()->SetActive(false,false, false);
		pP2Shared->SetCurrentHandObjectDrawn(false);
		pP2Shared->GetInsanityCollapse()->Stop();
	}

	//////////////////////////////////
	//Progress log
	gpBase->mpProgressLogHandler->AddLog(eLuxProgressLogLevel_High, "Player died!");

	//////////////////////////////////
	//Set some player stuff
	gpBase->mpInputHandler->ChangeState(eLuxInputState_Game);

	mpPlayer->ChangeState(eLuxPlayerState_Normal);
	mpPlayer->ChangeMoveState(eLuxMoveState_Normal);
	mpPlayer->GetHelperLantern()->SetActive(false,false, false);
	mpPlayer->SetCurrentHandObjectDrawn(false);
	mpPlayer->GetInsanityCollapse()->Stop();

	//////////////////////////////////
	//Text
	msCurrentHintText = msHintCat != "" ? kTranslate(msHintCat, msHintEntry) : kTranslate("Hints", "DefaultDeath");

	//////////////////////////////////
	// HARDMODE
	if (gpBase->mbHardMode && (msHintEntry != "DeathGrunt_22_Chancel"))
	{
		msCurrentHintText = kTranslate("Hints", "HardModeDeath");
	}

	//////////////////////////////////
	//Sound
	for(int i=0; i<=gpBase->mpMusicHandler->GetMaxPrio(); ++i)
	{
		gpBase->mpMusicHandler->Stop(0.2f,i);
	}

	if(mbSkipStartSound == false)
		gpBase->mpHelpFuncs->PlayGuiSoundData(msStartSound, eSoundEntryType_World);
	mbSkipStartSound = false;
	
	cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	pSoundHandler->FadeGlobalVolume(0, 0.15f,eSoundEntryType_World,eLuxGlobalVolumeType_Death,false);
	pSoundHandler->FadeGlobalSpeed(0.5f, 0.125f,eSoundEntryType_World,eLuxGlobalVolumeType_Death,false);
	
	///////////////////////////
	// Broadcast to all enemies
	gpBase->mpMapHandler->GetCurrentMap()->BroadcastEnemyMessage(eLuxEnemyMessage_PlayerDead, false,0,0);

	///////////////////////////
	// Reset variables
	mfHeightAdd =0;
	mfRoll =0;

	mfMinHeightAdd = mfHeightAddGoal;

	if(mpPlayer->GetCurrentMoveState() == eLuxMoveState_Normal)
	{
		cLuxMoveState_Normal *pNormalMove = static_cast<cLuxMoveState_Normal*>(mpPlayer->GetCurrentMoveStateData());
		if(pNormalMove->IsCrouching()) mfMinHeightAdd = mfHeightAddGoalCrouch;
	}

	mlState =0;

	mfFadeAlpha =0;
	mfWhiteCount =0;
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::Update(float afTimeStep)
{
	if(mbActive==false) return;

	mfT += afTimeStep;
	mFlashOscill.Update(afTimeStep);

	//////////////////////
	// Coop: death is shared and this helper is the single authoritative
	// sequence (see Start()), so the co-victim's CAMERA gets the same fall --
	// same height drop, same roll. Their alphas need no mirror: OnDraw paints
	// the fade and hint onto both hud sets from the one set of values.
	cLuxPlayer *pCoopVictim = NULL;
	if(gpBase->mpMapHandler->GetCoopMode() && gpBase->mpPlayer2 &&
		gpBase->mpPlayer2 != mpPlayer && gpBase->mpPlayer2->GetCharacterBody())
	{
		pCoopVictim = gpBase->mpPlayer2;
	}

	//////////////////////
	// Height add
	if(mlState ==0 || mlState==1)
	{
		if(mfHeightAdd > mfMinHeightAdd)
		{
			mfHeightAdd-= mfHeightAddSpeed*afTimeStep;
			if(mfHeightAdd < mfMinHeightAdd)
			{
				mfHeightAdd = mfMinHeightAdd;
			}
			mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_Death, cVector3f(0,mfHeightAdd,0));
			if(pCoopVictim) pCoopVictim->SetHeadPosAdd(eLuxHeadPosAdd_Death, cVector3f(0,mfHeightAdd,0));
			mlState = 1;
		}

		//////////////////////
		// Roll
		mfRoll += cMath::ToRad(mfRollSpeed)*afTimeStep;
		if(mfRoll > cMath::ToRad(65.0f)) mfRoll = cMath::ToRad(65.0f);

		mpPlayer->FadeRollTo(mfRoll, 10,10);
		if(pCoopVictim) pCoopVictim->FadeRollTo(mfRoll, 10,10);
	}
	//////////////////////
	// Fade Out
	if(mlState==1)
	{
		mfFadeAlpha += afTimeStep * (1.0f/ mfFadeOutTime);
		if(mfFadeAlpha > 1)
		{
			mfFadeAlpha = 1;

			if(mpPlayer->UsePermaDeath())
			{
				mlState = 4;

				cLuxSoundExtraData extraData;
				if(gpBase->mpHelpFuncs->PlayGuiSoundData(mpPlayer->GetCurrentPermaDeathSound(), eSoundEntryType_Gui, 1, eSoundEntityType_Main, true, &extraData))
				{
					mpVoiceEntry = extraData.mpSoundEntry;
					mlVoiceEntryId = mpVoiceEntry->GetId();
				}
			}
		}

		if(mfFadeAlpha > 0.75f)
		{
			if(!mpPlayer->UsePermaDeath())
			{
				mfTextAlpha1 += (1 - mfTextAlpha1) * 0.5f * afTimeStep;
				if(mfTextAlpha1 > 1) mfTextAlpha1 = 1;
			}
		}
        
		if(mfTextAlpha1>1) mfTextAlpha1 = 1;

		if(mfTextAlpha1 > 0.9f && mfFadeAlpha==1)
		{
			mfTextOnScreenCount += afTimeStep;
			if(mfTextOnScreenCount > 5.5f || mbShowHint==false)
			{
				mlState = 2;

				ResetGame();
				//Coop: BOTH players lived the whole sequence now -- the fall, the
				//fade and the hint are mirrored onto the co-victim's camera and hud
				//set -- so the respawn flash goes to both halves too. A flash with
				//no player context covers both; outside coop it is P1 alone either
				//way, so single player is untouched.
				if(pCoopVictim)
					gpBase->mpEffectHandler->GetFlash()->Start(0.7f, 1.2f, 2.5f);
				else
					gpBase->mpEffectHandler->GetFlash()->StartForPlayer(mpPlayer, 0.7f, 1.2f, 2.5f);
				gpBase->mpHelpFuncs->PlayGuiSoundData(msAwakenSound, eSoundEntryType_Gui);

				/////////////////////////////
				//// HARDMODE
				//if (gpBase->mbHardMode && gpBase->mpPlayer->IsActive())
				//{
				//	mlState = 5;
				//}
			}
		}
	}
	//////////////////////
	// Fade to white
	if(mlState == 2)
	{
		mfWhiteCount += afTimeStep;
		if(mfWhiteCount >= 0.5f)
		{
			mlState = 3;			
		}
	}
	//////////////////////
	// Fade In
	if(mlState == 3)
	{
		mfTextAlpha1 -= afTimeStep*0.85f;
		mfFadeAlpha -= afTimeStep*0.75f;
		if(mfFadeAlpha < 0)
		{
			mfFadeAlpha = 0;
						
			mbActive = false;

			//Save the current cat and entry.
			tString sTempCat = msHintCat;
			tString sTempEntry = msHintEntry;

			Reset();

			msHintCat = sTempCat;
			msHintEntry = sTempEntry;

			//////////////////////
			// HARDMODE
			if (gpBase->mbHardMode && gpBase->mpPlayer->IsActive()) // Check if active since player is set inactive in ambush
			{
				//////////////////////
				// Load latest save

				if (gpBase->mpSaveHandler->AutoLoad(true) == false)
				{
					mbToMainMenu = true;
				}
			}
		}
	}
	//////////////////////
	// Play voice
	if(mlState == 4)
	{
		cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
		if(mpVoiceEntry==NULL || pSoundHandler->IsValid(mpVoiceEntry, mlVoiceEntryId)==false)
		{
			gpBase->mpEngine->Exit();
		}
	}
	//////////////////////
	// HARDMODE
	if (mlState == 5)
	{
		//////////////////////
		// Load latest save
		if (gpBase->mpSaveHandler->AutoLoad(true) == false)
		{
			mbToMainMenu = true;
		}
	}
}


void cLuxPlayerDeath::PostUpdate(float afTimeStep)
{
	///////////////////////////
	// HARDMODE
	if (gpBase->mbHardMode == false) return;

	///////////////////////////
	// Wants to go to main menu, 
	// cant be in update since the playerclass will go bananas
	if (mbToMainMenu == false) return;
	
	//////////////////////////
	// Set MainMenu container
	gpBase->mpEngine->GetUpdater()->SetContainer("MainMenu");
	gpBase->mpLoadScreenHandler->DrawMenuScreen();

	//Reset game
	gpBase->mpEngine->GetUpdater()->BroadcastMessageToAll(eUpdateableMessage_Reset);
	gpBase->SetCustomStory(NULL);

	//Start up menu again
	gpBase->mpMainMenu->OnLeaveContainer("");
	gpBase->mpMainMenu->OnEnterContainer("");

	mbToMainMenu = false;
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::OnDraw(float afFrameTime)
{
	//Black flicker hunt -- temporary, same shape as the one in cLuxEffect_Fade.
	{
		static float sfPrevMul = 1.0f;
		const float fMul = mbActive ? (1-mfFadeAlpha) : 1.0f;
		if(sfPrevMul > 0.6f && fMul < 0.3f)
			Log("=== DARKENED BY cLuxPlayerDeath: screen mul %.3f -> %.3f (fadeAlpha %.3f)\n",
				sfPrevMul, fMul, mfFadeAlpha);
		sfPrevMul = fMul;
	}

	if(mbActive==false) return;

	////////////////////////////////////////////////////////////////////////
	// Coop: this helper is the single authoritative death sequence (see
	// Start()), so the fade and the hint are painted onto BOTH players' hud
	// sets from here -- the co-victim lives the same death on their own half,
	// exactly like Player 1. Either set can be transiently NULL (coop toggle,
	// split change, teardown): draw nowhere rather than on the wrong screen.
	cGuiSet *pSets[2] = { GetDrawHudSet(), NULL };
	if(gpBase->mpMapHandler->GetCoopMode() && gpBase->mpPlayer2 && mpPlayer->IsPlayer2()==false)
	{
		pSets[1] = gpBase->mpMapHandler->GetCoopHudSet();
		if(pSets[1] == pSets[0]) pSets[1] = NULL;	//never double-draw one set
	}

	////////////////////////////////////////////////////////////////////////
	// Hint text rows -- prepared once, drawn per set.
	cVector2f vFontSize = 32;
	float fSizeMul = 1.5f;
	float fSizeMulExtra =  (1- mfTextAlpha1)*4;
	float fMul = fSizeMul + fSizeMulExtra;

	tWStringVec vRows;
	if(mbShowHint)
		mpFont->GetWordWrapRows(550,vFontSize.y, vFontSize,msCurrentHintText,&vRows);

	for(int lSet=0; lSet<2; ++lSet)
	{
		cGuiSet *pHudSet = pSets[lSet];
		if(pHudSet==NULL) continue;

		//The full-screen modulative fade.
		pHudSet->DrawGfx(mpWhiteModGfx,gpBase->mvHudVirtualStartPos+ cVector3f(0,0,3), gpBase->mvHudVirtualSize,cColor(1-mfFadeAlpha,1));

		if(mbShowHint==false) continue; //Skip death hint

		float fY = 300;
		for(size_t row=0; row<vRows.size(); ++row)
		{
			tWString &sStr = vRows[row];
			float fSize = mpFont->GetLength(vFontSize* fMul, sStr.c_str());
			float fX = 400 - (fSize/2.0f);

			for(size_t i=0;i<sStr.length(); ++i)
			{
				float fTAdd = 0.3f * (float)i;
				float fYAdd = sin(mfT*0.5f + fTAdd) * 5.0f + cos(mfT*0.97f - fTAdd*2.73f) * 3.5f;

				tWString sChar = cString::SubW(sStr, (int)i, 1);
				cVector3f vPos = cVector3f(fX, fY + fYAdd - vFontSize.y*0.5f*fMul, 6);
				cVector2f vSize = cVector2f(vFontSize.x, vFontSize.y + fabs(fYAdd)*1.5f) * fMul;

				pHudSet->DrawFont(sChar, mpFont,vPos,vSize, cColor(1, mfTextAlpha1),eFontAlign_Left);

				float fBlurAlpha = 0;
				if(mfTextAlpha1 < 0.5f)	fBlurAlpha = mfTextAlpha1 / 0.5f;
				else					fBlurAlpha = 1 - (mfTextAlpha1-0.5f) / 0.5f;

				pHudSet->DrawFont(	sChar, mpFont,vPos + cVector3f(vSize.x*0.05f,vSize.y*0.05f, -1), vSize*1.1f, cColor(1, fBlurAlpha * 0.3f ),eFontAlign_Right);
				pHudSet->DrawFont(	sChar, mpFont,vPos + cVector3f(vSize.x*0.15f,vSize.y*0.15f, -2), vSize*1.3f, cColor(1, fBlurAlpha * 0.2f ),eFontAlign_Right);

				fX += mpFont->GetLength(vFontSize* fMul, sChar.c_str());
			}

			fY += vFontSize.y * 1.35f;
		}
	}

	//gpBase->mpGameHudSet->DrawFont(kTranslate("Game", "DeathPress"),mpFont, cVector3f(400, 550, 6), 17, cColor(1,mFlashOscill.val,mFlashOscill.val, mfTextAlpha2),eFontAlign_Center);
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::OnPressButton()
{
	if(mbActive==false || mfFadeAlpha < 1) return;

	mfTextOnScreenCount = 100;

    /*if(gpBase->mpSaveHandler->AutoLoad())
	{
		//////////////////////
		// Increase player attributes if low
		float fHealth = mpPlayer->GetHealth();
		float fSanity = mpPlayer->GetSanity();
		float fOil = mpPlayer->GetLampOil();

		if(fHealth < mfMaxHealthGain)
		{
			fHealth = cMath::RandRectf(cMath::Max(fHealth, mfMinHealthGain), mfMaxHealthGain);
		}
		if(fSanity < mfMaxSanityGain)
		{
			fSanity = cMath::RandRectf(cMath::Max(fSanity, mfMinSanityGain), mfMaxSanityGain);
		}
		if(fOil < mfMaxHealthGain)
		{
			fOil = cMath::RandRectf(cMath::Max(fOil, mfMinOilGain), mfMaxOilGain);
		}

		mpPlayer->SetHealth(fHealth);
		mpPlayer->SetSanity(fSanity);
		mpPlayer->SetLampOil(fOil);
	}*/
}

//-----------------------------------------------------------------------

void cLuxPlayerDeath::SetHint(const tString& asCat, const tString& asEntry)
{
	msHintCat = asCat;
	msHintEntry = asEntry;
}

void cLuxPlayerDeath::ResetGame()
{
	/////////////////////////////////////
	// Player
	mpPlayer->SetRoll(0);
	mpPlayer->GetCamera()->SetRoll(0);
	mpPlayer->GetCamera()->SetPitch(0);
	mpPlayer->SetHeadPosAdd(eLuxHeadPosAdd_Death, cVector3f(0,0,0));
	mpPlayer->SetCurrentHandObjectDrawn(true);
	mpPlayer->GetCharacterBody()->SetForceVelocity(0);
	mpPlayer->GetCharacterBody()->SetMoveSpeed(eCharDir_Forward,0);
	mpPlayer->GetCharacterBody()->SetMoveSpeed(eCharDir_Right,0);
	
	mpPlayer->SetHealth(50.0f);
	if(mpPlayer->GetSanity()<40.0f) mpPlayer->SetSanity(40.0f);

	/////////////////////////////////////
	// Coop: revive and reset Player 2 too — both players restart together.
	// (The checkpoint load below places both at the checkpoint start pos.)
	cLuxPlayer *pP2 = gpBase->mpPlayer2;
	if(gpBase->mpMapHandler->GetCoopMode() && pP2 && pP2->GetCharacterBody())
	{
		pP2->SetRoll(0);
		pP2->GetCamera()->SetRoll(0);
		pP2->GetCamera()->SetPitch(0);
		pP2->SetHeadPosAdd(eLuxHeadPosAdd_Death, cVector3f(0,0,0));
		pP2->GetCharacterBody()->SetForceVelocity(0);
		pP2->GetCharacterBody()->SetMoveSpeed(eCharDir_Forward,0);
		pP2->GetCharacterBody()->SetMoveSpeed(eCharDir_Right,0);

		pP2->SetHealth(50.0f);
		if(pP2->GetSanity()<40.0f) pP2->SetSanity(40.0f);
	}

	//////////////////////////////////
	//Sound
	cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	pSoundHandler->FadeGlobalVolume(1, 1,eSoundEntryType_World,eLuxGlobalVolumeType_Death,false);
	pSoundHandler->FadeGlobalSpeed(1, 0.5,eSoundEntryType_World,eLuxGlobalVolumeType_Death,false);

	//////////////////////////////////
	//Check point
	gpBase->mpMapHandler->GetCurrentMap()->LoadCheckPoint();	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PLAYER LEAN
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerLean::cLuxPlayerLean(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "PlayerLean")
{
	mfMaxMovement = 0.5f;
	mfMaxRotation = cMath::ToRad(15);

	mpHeadShape = NULL;
}

cLuxPlayerLean::~cLuxPlayerLean()
{

}

//-----------------------------------------------------------------------

void cLuxPlayerLean::Reset()
{
	mfDir = 0;
	mfDirAdd = 0;
	mfMaxTime = 0.8f;
	mfMovement = 0;
	mfRotation =0;

	mfMoveSpeed = 0;

	mbPressed = false;

	mpHeadShape = NULL;
}

//-----------------------------------------------------------------------

void cLuxPlayerLean::CreateWorldEntities(cLuxMap *apMap)
{
	iPhysicsWorld *pPhysicsWorld = apMap->GetPhysicsWorld();

	float fRadius = mpPlayer->GetCharacterBody()->GetSize().x/2 * 0.68f;
	float fHeight = 0.05f * 2;
	if(fHeight < 0)fHeight = fHeight * -1;
	cMatrixf mtxOffset = cMath::MatrixRotateZ(kPi2f);
	mpHeadShape = pPhysicsWorld->CreateCylinderShape(fRadius,fHeight,&mtxOffset);
}

void cLuxPlayerLean::DestroyWorldEntities(cLuxMap *apMap)
{
	iPhysicsWorld *pPhysicsWorld = apMap->GetPhysicsWorld();

	if(mpHeadShape) pPhysicsWorld->DestroyShape(mpHeadShape);
	mpHeadShape = NULL;
}


//-----------------------------------------------------------------------

void cLuxPlayerLean::Update(float afTimeStep)
{
	if(mpPlayer->IsDead()) return;

	////////////////////////////////
	//If pressed move in direction
	if(mbPressed)
	{
		float fDir = mfDir + mfDirAdd;

		mbPressed = false;

		float fGoalPos = mfMaxMovement * fDir;
		float fGoalRot = mfMaxRotation * -fDir;

		//////////////
		//Position
		float fPrevMovement = mfMovement;
		float fMoveSpeed = (fGoalPos - mfMovement);
		if(fabsf(fMoveSpeed) <0.1f) fMoveSpeed = 0.1f*fDir;
		mfMovement += fMoveSpeed * afTimeStep * 3;

		if(fGoalPos < 0 && mfMovement < fGoalPos) mfMovement =fGoalPos;
		if(fGoalPos > 0 && mfMovement > fGoalPos) mfMovement =fGoalPos;

		//////////////
		//Rotation
		float fPrevRotation = mfRotation;
		float fRotSpeed = fGoalRot - mfRotation;
		if(fabsf(fRotSpeed) <0.13f) fRotSpeed = 0.13f*-fDir;

		mfRotation += fRotSpeed * afTimeStep * 2;

		if(fGoalRot < 0 && mfRotation < fGoalRot) mfRotation = fGoalRot;
		if(fGoalRot > 0 && mfRotation > fGoalRot) mfRotation = fGoalRot;

		////////////////////
		//Check collision
		cCamera *pCam = mpPlayer->GetCamera();
		cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
		iPhysicsWorld *pPhysicsWorld = pMap->GetPhysicsWorld();

		float fReverseMov = fPrevMovement - mfMovement;
		float fReverseRot = fPrevRotation - mfRotation;

		iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
		float fHeightAdd = pCharBody->GetSize().y + mpPlayer->GetCameraPosAdd().y;
		cVector3f vStartPos = pCharBody->GetFeetPosition() + cVector3f(0,fHeightAdd,0);

		cVector3f vPos = vStartPos + pCam->GetRight() * mfMovement;

		int lCount = 0;
		while(pPhysicsWorld->CheckShapeWorldCollision(NULL,mpHeadShape, cMath::MatrixTranslate(vPos),NULL,false,true,NULL,false))
		{
			mfMovement += fReverseMov;
			mfRotation += fReverseRot;

			if(fReverseMov < 0 && mfMovement <0) {
				mfMovement =0;
				mfRotation =0;
				break;
			}
			if(fReverseMov > 0 && mfMovement >0){
				mfMovement =0;
				mfRotation =0;
				break;
			}

			vPos = vStartPos + pCam->GetRight() * mfMovement;
			lCount++;
			if(lCount >10){
				mfMovement =0;
				mfRotation =0;
				break;
			}
		}

		mpPlayer->FadeLeanRollTo(mfRotation, 5,3);
		mpPlayer->MoveHeadPosAdd(eLuxHeadPosAdd_Lean, cVector3f(mfMovement,0,0),2,0.05f);
	}
	////////////////////////////
	// Not pressed move back
	else if (mfMovement !=0 || mfRotation != 0)
	{
		mfRotation =0;
		mfMovement =0;
		mfDir = 0;
		mfDirAdd = 0;

		mpPlayer->FadeLeanRollTo(0, 4,2);
		mpPlayer->MoveHeadPosAdd(eLuxHeadPosAdd_Lean, cVector3f(0,0,0), 1.3f, 0.1f);
	}
}

//-----------------------------------------------------------------------


void cLuxPlayerLean::SetLean(float afMul)
{
	mfDir = cMath::Clamp(afMul, -1.0f, 1.0f);
	if (fabsf(afMul) > 0)
	{
		mfDirAdd = 0;
		mbPressed = true;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerLean::AddLean(float afAdd)
{
	mfDirAdd = cMath::Clamp(mfDirAdd+afAdd, -1.0f, 1.0f);
	mbPressed = fabsf(mfDirAdd) > 0;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER HUD EFFECT
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerHudEffect::cLuxPlayerHudEffect(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerHudEffect")
{
	cGui *pGui = gpBase->mpEngine->GetGui();

	//////////////////////////////////
	// Create damage types
	mvDamageTypes.resize(eLuxDamageType_LastEnum);

	LoadDamageData(&mvDamageTypes[eLuxDamageType_BloodSplat], "bloodsplat");
	LoadDamageData(&mvDamageTypes[eLuxDamageType_Claws], "claws");
	LoadDamageData(&mvDamageTypes[eLuxDamageType_Slash], "slash");

	//////////////////////////////////
	// Create flash gfx
	mpFlashGfx = pGui->CreateGfxFilledRect(cColor(1,1),eGuiMaterial_Alpha);
}

cLuxPlayerHudEffect::~cLuxPlayerHudEffect()
{

}
//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::AddDamageSplash(eLuxDamageType aType)
{
	
	//////////////////////////
	//Pick image to use
	cLuxPlayerDamageData *pDamageData = &mvDamageTypes[aType];
	if(pDamageData->mvImages.empty()) return;
	int lImageNum = cMath::RandRectl(0, (int)pDamageData->mvImages.size()-1);
	
	cGuiGfxElement *pGfxElem = pDamageData->mvImages[lImageNum];
	cVector2f vImageSize = pGfxElem->GetActiveSize();
	cVector2f vPos = cMath::RandRectVector2f(0, gpBase->mvHudVirtualCenterSize-vImageSize);

	//////////////////////////
	//Set properties
	for(int i=0; i<2; ++i)
	{
		cLuxPlayerHudEffect_Splash splash;

		splash.mpImage = pGfxElem;

		splash.mvPos = cVector3f(vPos.x, vPos.y, 1);
		
		splash.mvSize = vImageSize;
		splash.mfAlpha = 1.0f;

		if(i==0)
		{
			splash.mfAlphaMul = 0.3f;
			splash.mvPosVel = cVector3f(0,1.0f, 0);
			splash.mvSizeVel = cVector2f(0, 8.0f);
		}
		else
		{
			splash.mfAlphaMul = 0.7f;
			splash.mvPosVel = cVector3f(0,6.0f, 0);
			splash.mvSizeVel = cVector2f(0, 16.0f);
		}
		
		splash.mfAlphaVel = 0.4f;
		splash.mfAlphaMoveStart = 0.8f;

		mlstSplashes.push_back(splash);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::Flash(const cColor& aColor, eGuiMaterial aFlashMaterial, float afInTime, float afOutTime)
{
	mFlashColor = aColor;
	mFlashMaterial = aFlashMaterial;
	mfFlashAlpha = 0.0f;
	mfFlashAlphaSpeed = afInTime==0 ? 100000.0f : 1.0f / afInTime;
	mfFlashAlphaOutSpeed = afOutTime==0 ? -100000.0f : -1.0f / afOutTime;
	mbFlashActive = true;
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::OnDraw(float afFrameTime)
{
	DrawSplashes(afFrameTime);
	DrawFlash(afFrameTime);
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::Update(float afTimeStep)
{
	UpdateSplashes(afTimeStep);
	UpdateFlash(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::Reset()
{
	mlstSplashes.clear();
	mfFlashAlpha =0;
	mbFlashActive = false;
}

//-----------------------------------------------------------------------

cGuiSet* iLuxPlayerHelper::GetDrawHudSet()
{
	//Moved up from cLuxPlayerHudEffect. Player 2 only has a set of its own while
	//split-screen is running.
	if(mpPlayer && mpPlayer->IsPlayer2())
	{
		//NULL, not P1's set. Falling back to mpGameHudSet meant that any frame
		//where P2's helpers drew while the coop hud set was transiently gone --
		//coop toggling, split-mode change, map-change teardown -- landed P2's
		//full-screen overlays on PLAYER 1's screen. Callers must skip drawing
		//when this returns NULL.
		return gpBase->mpMapHandler->GetCoopHudSet();
	}
	return gpBase->mpGameHudSet;
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::DrawSplashes(float afFrameTime)
{
	cGuiSet *pHudSet = GetDrawHudSet();
	if(pHudSet==NULL) return;	//P2 with no coop hud set this frame -- draw nowhere, not on P1.

	cLuxPlayerHudEffect_SplashListIt it = mlstSplashes.begin();
	for(; it != mlstSplashes.end(); ++it)
	{
		cLuxPlayerHudEffect_Splash *pSplash = &(*it);

        pHudSet->DrawGfx(pSplash->mpImage, pSplash->mvPos,pSplash->mvSize, cColor(1, pSplash->mfAlpha * pSplash->mfAlphaMul));
	}
}

void cLuxPlayerHudEffect::UpdateSplashes(float afTimeStep)
{
	cLuxPlayerHudEffect_SplashListIt it = mlstSplashes.begin();
	for(; it != mlstSplashes.end();)
	{
		cLuxPlayerHudEffect_Splash *pSplash = &(*it);

		if(pSplash->mfAlpha < pSplash->mfAlphaMoveStart)
		{
			pSplash->mvPos += pSplash->mvPosVel * afTimeStep;
			pSplash->mvSize += pSplash->mvSizeVel * afTimeStep;
		}
		
		pSplash->mfAlpha -= pSplash->mfAlphaVel * afTimeStep;
		if(pSplash->mfAlpha < 0)
		{
			it = mlstSplashes.erase(it);
			continue;
		}

		++it;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::DrawFlash(float afFrameTime)
{
	if(mfFlashAlpha <=0 || mbFlashActive==false) return;

	cGuiSet *pHudSet = GetDrawHudSet();
	if(pHudSet==NULL) return;	//P2 with no coop hud set this frame -- draw nowhere, not on P1.

	cColor col = mFlashColor;

	col.a *= mfFlashAlpha;

	pHudSet->DrawGfx(mpFlashGfx,gpBase->mvHudVirtualStartPos +cVector3f(0,0,0), gpBase->mvHudVirtualSize, col, mFlashMaterial);
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::UpdateFlash(float afTimeStep)
{
	if(mbFlashActive==false) return;
	
	mfFlashAlpha += mfFlashAlphaSpeed * afTimeStep;

	if(mfFlashAlpha > 1.0f && mfFlashAlphaSpeed > 0)
	{
		mfFlashAlpha = 1.0f;
		mfFlashAlphaSpeed = mfFlashAlphaOutSpeed;
	}
	if(mfFlashAlpha < 0 && mfFlashAlphaSpeed < 0)
	{
		mfFlashAlpha = 0;
		mbFlashActive = false;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHudEffect::LoadDamageData(cLuxPlayerDamageData *apData, const tString& asName)
{
	cGui *pGui = gpBase->mpEngine->GetGui();
	cFileSearcher *pFileSearcher = gpBase->mpEngine->GetResources()->GetFileSearcher();

	tString sFileNameBase = "graphics/hud/damage_"+asName;

	int lCount = 0;
	tString sFile = sFileNameBase + cString::ToString(lCount)+".tga";

	while(pFileSearcher->GetFilePath(sFile) != _W(""))
	{
		cGuiGfxElement *pGfx = pGui->CreateGfxImage(sFile, eGuiMaterial_Alpha);
		if(pGfx) apData->mvImages.push_back(pGfx);

		lCount++;
		sFile = sFileNameBase + cString::ToString(lCount)+".tga";
	}
}


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PLAYER LIGHT LEVEL
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerLightLevel::cLuxPlayerLightLevel(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerLightLevel")
{
	mfRadiusAdd = gpBase->mpGameCfg->GetFloat("Player_Darkness","RadiusAdd",0);
}

cLuxPlayerLightLevel::~cLuxPlayerLightLevel()
{
}

//-----------------------------------------------------------------------

void cLuxPlayerLightLevel::OnStart()
{
	
}

void cLuxPlayerLightLevel::Reset()
{
	mfExtendedLightLevel = 1.0f;
	mfNormalLightLevel = 1.0f;
	mfUpdateCount =0;
}

//-----------------------------------------------------------------------

void cLuxPlayerLightLevel::Update(float afTimeStep)
{
	///////////////////////////////////////
	//If count reaches 0, update light level
	if(mfUpdateCount <=0.0f)
	{
		mfUpdateCount = 1.0f / 2.0f;

		////////////////////////////////
		//Get character body properties
		iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
		cVector3f vPos = pCharBody->GetPosition();
		cVector3f vSize = pCharBody->GetSize();
		cVector3f vForward = pCharBody->GetForward();

		////////////////////////////////
		//Set up positions to test light level at.
		const int lTestPos =5;
		cVector3f vTestPos[lTestPos] =
		{
			vPos,	 //Center
			vPos + cVector3f(0,vSize.y-0.1f, 0),	//Above feet
			vPos - cVector3f(0,vSize.y-0.1f, 0),	//Head
			vPos + vForward * vSize.z*0.8f,			//In front of center
			vPos - cVector3f(0,vSize.y-0.1f, 0) + vForward * vSize.z*0.8f //In front of feet.
		};

		////////////////////////////////
		//Get lights to skip
		std::vector<iLight*> vSkipLights;
		vSkipLights.push_back(mpPlayer->GetHelperInDarkness()->GetAmbientLight());
		
		////////////////////////////////
		//Get light level at all positions and then calculate median.
		//float fTotalLight =0;
		
		mfExtendedLightLevel = 0.0f;
		mfNormalLightLevel = 0.0f;
		if(mpPlayer->GetHelperLantern()->IsActive())
		{
			mfExtendedLightLevel += 1.0f;
			mfNormalLightLevel += 1.0f;
		}
		
		for(int i=0; i<lTestPos; ++i)
		{
       		//fTotalLight += gpBase->mpMapHelper->GetLightLevelAtPos(vTestPos[i], &vSkipLights);
			float fExtLight = gpBase->mpMapHelper->GetLightLevelAtPos(vTestPos[i], &vSkipLights, mfRadiusAdd);
			float fNormalLight = gpBase->mpMapHelper->GetLightLevelAtPos(vTestPos[i], &vSkipLights, 0);
			
			mfExtendedLightLevel = cMath::Max(fExtLight, mfExtendedLightLevel);
			mfNormalLightLevel = cMath::Max(fNormalLight, mfNormalLightLevel);
		}

		//mfLightLevel = fTotalLight / (float)lTestPos;
	}
	else
	{
		mfUpdateCount -= afTimeStep;
	}
}


//-----------------------------------------------------------------------



void cLuxPlayerLightLevel::OnMapEnter(cLuxMap *apMap)
{
	mfUpdateCount =0;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER IN DARKNESS
//////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------

cLuxPlayerInDarkness::cLuxPlayerInDarkness(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerInDarkness")
{
	mpSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();

	mfMinDarknessLightLevel = gpBase->mpGameCfg->GetFloat("Player_Darkness","MinLightLevel",0);
	
	mfAmbientLightMinLightLevel = gpBase->mpGameCfg->GetFloat("Player_Darkness","AmbientLightMinLightLevel",0);
	mfAmbientLightRadius = gpBase->mpGameCfg->GetFloat("Player_Darkness","AmbientLightRadius",0);
	mfAmbientLightIntensity = gpBase->mpGameCfg->GetFloat("Player_Darkness","AmbientLightIntensity",0);
	mfAmbientLightFadeInTime = gpBase->mpGameCfg->GetFloat("Player_Darkness","AmbientLightFadeInTime",0);
	mfAmbientLightFadeOutTime = gpBase->mpGameCfg->GetFloat("Player_Darkness","AmbientLightFadeOutTime",0);
	mAmbientLightColor = gpBase->mpGameCfg->GetColor("Player_Darkness","AmbientLightColor",cColor(0));

	msLoopSoundFile  = mpPlayer->GetPlayerCfg()->GetString("Player_Darkness", "LoopSoundFile","");
	mfLoopSoundVolume = gpBase->mpGameCfg->GetFloat("Player_Darkness", "LoopSoundVolume",0);
	mfLoopSoundStartupTime = gpBase->mpGameCfg->GetFloat("Player_Darkness", "LoopSoundStartupTime",0);
	mfLoopSoundFadeInSpeed = gpBase->mpGameCfg->GetFloat("Player_Darkness", "LoopSoundFadeInSpeed",0);
	mfLoopSoundFadeOutSpeed = gpBase->mpGameCfg->GetFloat("Player_Darkness", "LoopSoundFadeOutSpeed",0);

	mfSanityLossPerSecond = gpBase->mpGameCfg->GetFloat("Player_Darkness", "SanityLossPerSecond",0);
}

cLuxPlayerInDarkness::~cLuxPlayerInDarkness()
{
	
}

//-----------------------------------------------------------------------

void cLuxPlayerInDarkness::OnStart()
{

}
void cLuxPlayerInDarkness::Reset()
{
	mbActive = true;

	mpAmbientLight =NULL;

	mbAmbientLightIsOn = false;
	mbInDarkness = false;

	mpLoopSound = NULL;
	mfLoopSoundCount =0;

	mfSanityLossMul =0;

	mfShowHintTimer = 0;
}

//-----------------------------------------------------------------------


void cLuxPlayerInDarkness::Update(float afTimeStep)
{
	if (!mbActive) return;

	///////////////////////
	// Get light level
    float fExtLightLevel = mpPlayer->GetHelperLightLevel()->GetExtendedLightLevel();
	float fNormalLightLevel = mpPlayer->GetHelperLightLevel()->GetNormalLightLevel();

	///////////////////////
	// Update ambient light position.
	cVector3f vCamPos = mpPlayer->GetCamera()->GetPosition();
	mpAmbientLight->SetPosition(vCamPos - cVector3f(0,0.3f,0));


	////////////////////////////
	//Turn off ambient light
	if(fNormalLightLevel > mfAmbientLightMinLightLevel)
	{
		if(mbAmbientLightIsOn)
		{
			mbAmbientLightIsOn = false;
			mpAmbientLight->FadeTo(cColor(0.0f, 0.0f),mpAmbientLight->GetRadius(),mfAmbientLightFadeOutTime);
		}
	}
	////////////////////////////
	//Turn on ambient light
	else
	{
		if(mbAmbientLightIsOn==false)
		{
			mbAmbientLightIsOn = true;

			////////////////////////
			// HARDMODE
			if (gpBase->mbHardMode)
				mpAmbientLight->FadeTo(mAmbientLightColor*mfAmbientLightIntensity * 0.75f , mpAmbientLight->GetRadius(), mfAmbientLightFadeInTime * 2.5f);
			else
				mpAmbientLight->FadeTo(mAmbientLightColor*mfAmbientLightIntensity, mpAmbientLight->GetRadius(), mfAmbientLightFadeInTime);

		}

	}

	///////////////////////
	// Light
    if(fExtLightLevel > mfMinDarknessLightLevel)
	{
		////////////////////////////
		//Turn off loop sound
		/*if(mpLoopSound)
		{
			if(mfLoopSoundCount <= 0)
			{
				mpLoopSound->FadeOut(mfLoopSoundFadeOutSpeed);
				mpLoopSound = NULL;
			}
		}*/
        
		mfLoopSoundCount-= afTimeStep;
		if(mfLoopSoundCount <= 0) mfLoopSoundCount = 0;


		mbInDarkness = false;
		
		mfSanityLossMul -= afTimeStep*0.3f;
		if(mfSanityLossMul < 0) mfSanityLossMul = 0;
	}
	///////////////////////
	// Darkness
	else
	{
		mfSanityLossMul += afTimeStep*0.1f;
		if(mfSanityLossMul > 1) mfSanityLossMul = 1;

		////////////////////////////
		//Lower sanity
		if(	mpPlayer->GetHelperFlashback()->IsActive()==false && mpPlayer->GetSanityDrainDisabled()==false && 
			gpBase->mpEffectHandler->GetEmotionFlash()->IsActive()==false)
		{
			mpPlayer->LowerSanity(mfSanityLossPerSecond*afTimeStep*mfSanityLossMul, true);

			if(mfShowHintTimer<=0 && mfSanityLossMul > 0.05f)
			{
				mfShowHintTimer = 3.0f;
				gpBase->mpHintHandler->Add("DarknessDecrease", kTranslate("Hints", "DarknessDecrease"), 0);
			}
			else
			{
				mfShowHintTimer -= afTimeStep;
			}
		}
				
		////////////////////////////
		//Check if sound should be played
		/*if(mpLoopSound == NULL)
		{
			if(mfLoopSoundCount >= mfLoopSoundStartupTime)
			{
				mpLoopSound = mpSoundHandler->PlayGuiStream(msLoopSoundFile,true,mfLoopSoundVolume);
				if(mpLoopSound) mpLoopSound->FadeIn(1.0f, mfLoopSoundFadeInSpeed);
			}
		}*/

		mfLoopSoundCount+= afTimeStep;
		if(mfLoopSoundCount >= mfLoopSoundStartupTime) mfLoopSoundCount = mfLoopSoundStartupTime;
		

		mbInDarkness = true;
	}

}

//-----------------------------------------------------------------------
void cLuxPlayerInDarkness::OnMapEnter(cLuxMap *apMap)
{
	mbAmbientLightIsOn = false;
	mbInDarkness = false;
}

void cLuxPlayerInDarkness::OnMapLeave(cLuxMap *apMap)
{

}

//-----------------------------------------------------------------------

void cLuxPlayerInDarkness::CreateWorldEntities(cLuxMap *apMap)
{
	cWorld *pWorld = apMap->GetWorld();

	tString sDarkName = mpPlayer->IsPlayer2() ? "PlayerDarknessAmbient_P2" : "PlayerDarknessAmbient";
	mpAmbientLight = pWorld->CreateLightPoint(sDarkName,"",false);
	mpAmbientLight->SetDiffuseColor(cColor(0.0f, 0.0f));

	mpAmbientLight->SetRadius(mfAmbientLightRadius);

	/////////////////////
	// HARDMODE
	if(gpBase->mbHardMode)
		mpAmbientLight->SetRadius(mfAmbientLightRadius*0.5f);


	mpAmbientLight->SetCastShadows(false);
	mpAmbientLight->SetIsSaved(false);
}

void cLuxPlayerInDarkness::DestroyWorldEntities(cLuxMap *apMap)
{
	if(mpAmbientLight) apMap->GetWorld()->DestroyLight(mpAmbientLight);
	mpAmbientLight = NULL;
}

//-----------------------------------------------------------------------

bool cLuxPlayerInDarkness::InDarkness()
{
	float fLightLevel = mpPlayer->GetHelperLightLevel()->GetExtendedLightLevel();
	return fLightLevel <= mfMinDarknessLightLevel;
}

//-----------------------------------------------------------------------

void cLuxPlayerInDarkness::SetActive(bool abX)
{
	mbActive = abX;

	if (abX == false)
	{
		if(mbAmbientLightIsOn)
		{
			mbAmbientLightIsOn = false;
			mpAmbientLight->FadeTo(cColor(0.0f, 0.0f),mpAmbientLight->GetRadius(),mfAmbientLightFadeOutTime);
		}
        
		mfLoopSoundCount = 0;
		mbInDarkness = false;
		mfSanityLossMul = 0;
	}
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// POSSESS AN ENEMY
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

//How far H reaches, and how far in front of the enemy we look for a door. The
//steering goal distance moved to LuxEnemy.cpp with the steering itself.
static const float gfPossessRange = 25.0f;
static const float gfPossessDoorRange = 2.5f;

/**
 * Ray for pulling the chase camera in when it would end up inside a wall.
 *
 * Skips characters on purpose: the ray starts inside the enemy we are possessing,
 * and treating its own capsule as scenery would jam the camera against its back.
 */
class cLuxPossessCamRayCallback : public iPhysicsRayCallback
{
public:
	void Reset(){ mfClosestDist = -1; }

	bool BeforeIntersect(iPhysicsBody *apBody)
	{
		if(apBody->GetCollide()==false) return false;
		if(apBody->IsCharacter()) return false;
		return true;
	}

	bool OnIntersect(iPhysicsBody *apBody, cPhysicsRayParams *apParams)
	{
		if(mfClosestDist < 0 || apParams->mfDist < mfClosestDist) mfClosestDist = apParams->mfDist;
		return true;
	}

	float mfClosestDist;
};

static cLuxPossessCamRayCallback gPossessCamRayCallback;

//-----------------------------------------------------------------------

/**
 * What each "Become <x>" spawns, in eLuxMorphType order.
 *
 * Paths are the shipped ones, not guesses: the grunt and the brute are the
 * files the co-op avatar rigs already load the meshes out of, the water lurker
 * is what the Cellar Archives map places, and the suitor lives in the Justine
 * content, which cLuxBase's own CRC checks name.
 *
 * A missing file is not a crash -- cLuxMap::CreateEntity logs it and no entity
 * appears, which Morph() reports as a refused morph.
 */
struct cLuxMorphDef
{
	const char *msName;
	const char *msEntFile;
};

static const cLuxMorphDef gvMorphDefs[eLuxMorphType_LastEnum] =
{
	{ "Grunt",        "entities/enemy/servant_grunt/servant_grunt.ent" },
	{ "Brute",        "entities/enemy/servant_brute/servant_brute.ent" },
	{ "Suitor",       "entities/ptest/enemy_suitor/enemy_suitor_alois.ent" },
	{ "Water Lurker", "entities/enemy/waterlurker/waterlurker.ent" },
};

//Only has to be unique among the entities alive at once; cLuxMap keys its name
//map on this, and a collision would make the second one unfindable.
static int glMorphSpawnCount = 0;

//-----------------------------------------------------------------------

cLuxPlayerPossess::cLuxPlayerPossess(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "PlayerPossess")
{
	mpEnemy = NULL;

	mlMorphType = -1;
	mpMorphMap = NULL;

	mpCam = NULL;
	mpBoundViewport = NULL;
	mpSavedCamera = NULL;

	mfCamYaw = 0;
	mfCamPitch = 0;

	mfWishForward = 0;
	mfWishRight = 0;
	mbRunning = false;
}

//-----------------------------------------------------------------------

cLuxPlayerPossess::~cLuxPlayerPossess()
{
	Release();

	if(mpCam)
	{
		gpBase->mpEngine->GetScene()->DestroyCamera(mpCam);
		mpCam = NULL;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerPossess::Reset()
{
	Release();
}

//-----------------------------------------------------------------------

void cLuxPlayerPossess::OnMapLeave(cLuxMap *apMap)
{
	//Non-negotiable. The co-op viewports have their camera nulled and are reused
	//across a map change, and are destroyed outright when co-op is switched off --
	//so holding a borrowed viewport across either is how you get a dangling pointer.
	Release();
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerPossess::Toggle()
{
	//Player 1 only, by request. Helpers are built for both players, and although
	//UpdatePlayer2Input only ever touches gpBase->mpPlayer2, a debug feature should
	//not rely on that staying true.
	if(mpPlayer->IsPlayer2()) return;

	if(mpEnemy)
	{
		Release();
		return;
	}

	if(::ImGuiDebugMenu::GetAllowPossession()==false) return;
	if(gpBase->mpMapHandler->GetCurrentMap()==NULL) return;
	if(mpPlayer->IsDead()) return;
	if(mpPlayer->GetCharacterBody()==NULL) return;

	iLuxEnemy *pEnemy = PickEnemyUnderCrosshair();
	if(pEnemy==NULL) return;
	if(pEnemy->GetCharacterBody()==NULL) return;
	if(pEnemy->GetHealth() <= 0) return;
	if(pEnemy->IsPossessed()) return;

	mpEnemy = pEnemy;
	mpEnemy->SetPossessed(true);

	//Start facing the way the enemy already faces, so taking it over does not snap
	//the view somewhere unrelated. Character-body yaw and camera yaw are the same
	//convention -- cLuxPlayer::AddYaw sets one straight from the other.
	mfCamYaw = mpEnemy->GetCharacterBody()->GetYaw();
	mfCamPitch = -cMath::ToRad(12.0f);

	mfWishForward = 0;
	mfWishRight = 0;
	mbRunning = false;

	BindCamera();
	PoseCamera();
}

//-----------------------------------------------------------------------

void cLuxPlayerPossess::ToggleMorph(int alMorphType)
{
	//Player 1 only, same as Toggle.
	if(mpPlayer->IsPlayer2()) return;
	if(alMorphType < 0 || alMorphType >= eLuxMorphType_LastEnum) return;

	//Same key twice changes back.
	if(mlMorphType == alMorphType)
	{
		Unmorph();
		return;
	}

	if(::ImGuiDebugMenu::GetAllowEnemyMorph()==false) return;

	//A different monster, or one while holding a possessed one: let go of whatever
	//we have first.
	//
	//Through Unmorph when we are morphed, not straight to Release: swapping monster
	//has to move our body to where the old one had walked to, or the new one would
	//spawn back at the spot the first morph happened and yank us across the level.
	if(IsMorphed())	Unmorph();
	else			Release();

	if(Morph(alMorphType)==false)
	{
		gpBase->mpDebugHandler->AddMessage(
			_W("Could not morph -- that monster's entity file is missing"), false);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerPossess::Unmorph()
{
	if(mlMorphType < 0) return;

	//Where the monster ended up, read BEFORE Release deletes it. The player's own
	//body never moved, so without this you would snap back to where you morphed.
	iCharacterBody *pEnemyBody = mpEnemy ? mpEnemy->GetCharacterBody() : NULL;
	iCharacterBody *pPlayerBody = mpPlayer->GetCharacterBody();

	const bool bMovePlayer = pEnemyBody != NULL && pPlayerBody != NULL;
	cVector3f vFeet(0,0,0);
	float fYaw = 0;
	if(bMovePlayer)
	{
		vFeet = pEnemyBody->GetFeetPosition();
		fYaw = pEnemyBody->GetYaw();
	}

	Release();

	if(bMovePlayer)
	{
		//The same sequence cLuxMapHandler::TeleportPlayerToOther uses, and for the
		//same reasons: clear the fall speed the parked body may have accumulated,
		//point the camera where the monster was looking, and run one tiny update so
		//the body settles against the floor instead of on the next real frame.
		pPlayerBody->SetFeetPosition(vFeet);
		pPlayerBody->SetForceVelocity(0);
		pPlayerBody->SetYaw(fYaw);
		if(mpPlayer->GetCamera()) mpPlayer->GetCamera()->SetYaw(fYaw);
		pPlayerBody->Update(0.001f);

		//A liquid area only clears the in-water flag for a body it can still see,
		//and ours was switched off the whole time -- so a morph that ended out of
		//the water would otherwise leave the player swimming on dry land.
		mpPlayer->SetIsInWater(false);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerPossess::AddMove(float afForward, float afRight)
{
	if(mpEnemy==NULL) return;

	mfWishForward += afForward;
	mfWishRight += afRight;
}

//-----------------------------------------------------------------------

void cLuxPlayerPossess::AddLook(float afYawAdd, float afPitchAdd)
{
	if(mpEnemy==NULL || mpCam==NULL) return;

	mfCamYaw += afYawAdd;
	mfCamPitch += afPitchAdd;

	//Clamped here rather than leaning on cCamera::SetPitch, so mfCamPitch cannot
	//wind up past the limit and then need unwinding before the view moves again.
	const float fLimit = cMath::ToRad(70.0f);
	if(mfCamPitch > fLimit) mfCamPitch = fLimit;
	if(mfCamPitch < -fLimit) mfCamPitch = -fLimit;
}

//-----------------------------------------------------------------------

void cLuxPlayerPossess::SetRunning(bool abX)
{
	mbRunning = abX;
}

//-----------------------------------------------------------------------

void cLuxPlayerPossess::PushSteerToEnemy()
{
	if(mpEnemy==NULL || mpCam==NULL) return;

	////////////////////////////////
	// Bring the camera up to date FIRST.
	//
	// PoseCamera runs at the end of our Update, which is later in the tick than
	// this -- so mpCam still holds LAST tick's angles right now, and reading its
	// forward would hand the monster a heading one tick of mouse movement stale.
	// SetYaw/SetPitch only dirty the view matrix; GetForward rebuilds it.
	mpCam->SetYaw(mfCamYaw);
	mpCam->SetPitch(mfCamPitch);

	//GetForward and GetRight are the engine's own pair -- cCamera::GetForward
	//negates the view matrix's forward and GetRight does not, matching what
	//MoveForward/MoveRight do off the move matrix. They agree by construction.
	cVector3f vCamFwd = mpCam->GetForward();
	cVector3f vCamRight = mpCam->GetRight();
	vCamFwd.y = 0;
	vCamRight.y = 0;

	cVector3f vAim(0,0,0);
	if(vCamFwd.SqrLength() > 0.0001f)
	{
		vAim = vCamFwd;
		vAim.Normalize();
	}

	////////////////////////////////
	// Steering direction
	cVector3f vWish(0,0,0);
	if(vAim.SqrLength() > 0.0001f && vCamRight.SqrLength() > 0.0001f &&
		(cMath::Abs(mfWishForward) > 0.01f || cMath::Abs(mfWishRight) > 0.01f))
	{
		vCamRight.Normalize();

		vWish = vAim*mfWishForward + vCamRight*mfWishRight;
		if(vWish.SqrLength() > 0.0001f)	vWish.Normalize();
		else							vWish = cVector3f(0,0,0);
	}

	mpEnemy->SetPossessSteer(vWish, vAim, mbRunning);

	mfWishForward = 0;
	mfWishRight = 0;
}

//-----------------------------------------------------------------------

void cLuxPlayerPossess::DoAttack()
{
	if(mpEnemy==NULL) return;

	//A state change, not a direct Attack(). The state plays the swing and the damage
	//arrives on the animation's own timing, as eLuxEnemyMessage_AnimationSpecialEvent
	//-> Attack(mNormalAttackSize, mNormalAttackDamage). Calling Attack() straight
	//would land a hit with no swing and no wind-up.
	//
	//ChangeState is a no-op while already in the state, so holding the button cannot
	//restart a swing half way through.

	////////////////////////////////
	// Which swing -- the monster's own answer, not ours.
	//
	// A grunt at a run does not stop and claw, it launches: cLuxEnemy_Grunt's Hunt
	// state picks AttackMeleeLong whenever the player is past NormalAttackDistance
	// and it is already running. There is no player to measure to here, so the
	// running half of that test is the whole test -- and it is read off the MOVER's
	// move state rather than off our own Shift flag, because that is derived from
	// real velocity against WalkToRunSpeed. Holding Shift in a doorway is not
	// running, and the monster knows it.
	//
	// HasLungeAttack gates it: the water lurker never implements the state, and
	// changing into a state an enemy does not implement strands it in something
	// with no Enter, no Update and no way out.
	const bool bLunge = mpEnemy->HasLungeAttack() &&
						mpEnemy->GetMover() != NULL &&
						mpEnemy->GetMover()->GetMoveState() == eLuxEnemyMoveState_Running;

	mpEnemy->ChangeState(bLunge ? eLuxEnemyState_AttackMeleeLong
								: eLuxEnemyState_AttackMeleeShort);
}

//-----------------------------------------------------------------------

void cLuxPlayerPossess::DoBreakDoor()
{
	if(mpEnemy==NULL) return;

	//Second press gives up on the door.
	if(mpEnemy->GetCurrentEnemyState() == eLuxEnemyState_BreakDoor)
	{
		mpEnemy->ChangeState(eLuxEnemyState_Idle);
		return;
	}

	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	iCharacterBody *pBody = mpEnemy->GetCharacterBody();
	if(pMap==NULL || pBody==NULL) return;

	iPhysicsWorld *pPhysicsWorld = pMap->GetPhysicsWorld();
	if(pPhysicsWorld==NULL) return;

	//Look for the door instead of waiting to be blocked by it.
	//iLuxEnemy::UpdateCheckStuckAtDoor only raises mbStuckAtDoor once the mover's
	//stuck counter has built up, which is no use to somebody who wants to smash a
	//door on purpose. Same test it makes, though: a prop, of type SwingDoor.
	const cVector3f vStart = pBody->GetPosition();

	gGunRayCallback.Reset();
	pPhysicsWorld->CastRay(&gGunRayCallback, vStart, vStart + pBody->GetForward()*gfPossessDoorRange,
						   true, false, false, true);

	iPhysicsBody *pHit = gGunRayCallback.mpClosestBody;
	if(pHit==NULL || pHit->IsCharacter()) return;

	iLuxEntity *pEntity = (iLuxEntity*)pHit->GetUserData();
	if(pEntity==NULL || pEntity->GetEntityType() != eLuxEntityType_Prop) return;

	iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
	if(pProp->GetPropType() != eLuxPropType_SwingDoor) return;

	//What the Hunt state sets before it hands over. mlStuckDoorID is the one that
	//matters: BreakDoor's AnimationOver handler asks mpMap->DoorIsBroken(mlStuckDoorID)
	//to decide whether to swing again or stop.
	mpEnemy->mlStuckDoorID = pProp->GetID();
	mpEnemy->mvTempPos = pHit->GetWorldPosition();

	mpEnemy->ChangeState(eLuxEnemyState_BreakDoor);
}

//-----------------------------------------------------------------------

void cLuxPlayerPossess::Update(float afTimeStep)
{
	////////////////////////////////
	// The debug menu's Become buttons.
	//
	// Taken here rather than in cLuxInputHandler because the menu is usable in
	// states where the player input update does not run, and because this is the
	// one function that is guaranteed to tick whenever the player does. The 1-4
	// keys call ToggleMorph directly; both ends up in the same place.
	//
	// Read unconditionally, above the early-out: a request parked while nothing is
	// held is exactly the normal case.
	const int lMorphRequest = ::ImGuiDebugMenu::ConsumeEnemyMorphRequest();
	if(lMorphRequest >= 0 && mpPlayer->IsPlayer2()==false) ToggleMorph(lMorphRequest);

	if(mpEnemy==NULL) return;

	////////////////////////////////
	// Every reason to let go on our own
	//
	// Possession's own switch does NOT release a morph -- morphing has its own
	// switch and its own keys, and needing both ticked to stay a monster would be
	// a trap. Each panic button covers its own feature.
	const bool bSwitchedOff = IsMorphed()
							? ::ImGuiDebugMenu::GetAllowEnemyMorph()==false
							: ::ImGuiDebugMenu::GetAllowPossession()==false;

	if(gpBase->mpMapHandler->GetCurrentMap()==NULL ||
		mpEnemy->GetCharacterBody()==NULL ||
		mpEnemy->GetHealth() <= 0 ||
		mpPlayer->GetCharacterBody()==NULL ||
		mpPlayer->IsDead() ||
		bSwitchedOff)
	{
		//Through Unmorph where it applies, so a monster that got killed under you
		//puts you down where it fell rather than back at the morph spot. Unmorph
		//copes with the map or either body already being gone.
		if(IsMorphed())	Unmorph();
		else			Release();
		return;
	}

	////////////////////////////////
	// Park Player 1's own body
	//
	// Parked, not deactivated. cLuxPlayer::SetActive(false) is purely an input gate
	// (cLuxInputHandler:1220) and we still need to read that input, and the avatar
	// visibility rules deliberately do NOT key on IsActive -- "a paused player is
	// still standing in the room" -- so P2 keeps seeing the body either way.
	// Zeroing the velocity every frame is the same idiom cLuxPlayerDeath::ResetGame
	// uses to stop a player dead.
	iCharacterBody *pPlayerBody = mpPlayer->GetCharacterBody();
	pPlayerBody->SetForceVelocity(0);
	pPlayerBody->SetMoveSpeed(eCharDir_Forward, 0);
	pPlayerBody->SetMoveSpeed(eCharDir_Right, 0);

	////////////////////////////////
	// Walk or run
	//
	// SetMoveSpeed only swaps four scalars (forward/backward speed and accel). The
	// run ANIMATION is never commanded: cLuxEnemyMover::UpdateMoveAnimation picks
	// walk vs run from measured velocity against mfWalkToRunSpeed, so raising the
	// cap makes the run cycle appear by itself.
	//
	// Re-asserted every frame because states leave it wherever suits them -- the
	// grunt's lunge sets Run in its kLuxOnLeave, so without this one launch attack
	// would leave you sprinting with Shift up for good.
	//
	// EXCEPT while a state is deliberately driving the speed itself. The lunge
	// multiplies mfForwardSpeed (1.5x on the grunt, 2x on the manpig) in its
	// kLuxOnEnter, and SetMoveSpeed reassigns that field outright from the defaults
	// -- so re-asserting through a launch attack flattened it back to an ordinary
	// run on the very next tick, which is exactly what a lunge is not.
	const eLuxEnemyState possessState = mpEnemy->GetCurrentEnemyState();
	const bool bStateOwnsSpeed = possessState == eLuxEnemyState_AttackMeleeShort ||
								 possessState == eLuxEnemyState_AttackMeleeLong ||
								 possessState == eLuxEnemyState_BreakDoor;

	if(bStateOwnsSpeed==false)
		mpEnemy->SetMoveSpeed(mbRunning ? eLuxEnemyMoveSpeed_Run : eLuxEnemyMoveSpeed_Walk);

	////////////////////////////////
	// Steering is NOT done here.
	//
	// PushSteerToEnemy hands the monster its goal from cLuxInputHandler, and
	// iLuxEnemy::OnUpdate consumes it in the slot the pathfinder would have filled.
	// Doing it here instead cost a full tick: this helper runs from cLuxPlayer,
	// which is behind cLuxMapHandler in the "Default" container, so the goal was
	// always one tick old by the time the mover turned on it and CalculateSpeedMul
	// braked on it. With TurnBreakMul at 2 a 29 degree heading error is already a
	// dead stop, so a heading permanently trailing the camera meant a monster that
	// braked its way round every corner.

	PoseCamera();
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerPossess::Release()
{
	////////////////////////////////
	// Our own body comes back first, before anything below can bail out on us --
	// leaving it switched off would strand the player unable to move and, in
	// co-op, invisible to the other one.
	//
	// Guarded on having been morphed rather than done every time: possession
	// never switches the body off, so a body found switched off on that path was
	// switched off by something else and is not ours to switch back on.
	if(mlMorphType >= 0)
	{
		iCharacterBody *pPlayerBody = mpPlayer->GetCharacterBody();
		if(pPlayerBody)
		{
			pPlayerBody->SetActive(true);
			pPlayerBody->SetForceVelocity(0);
		}
	}

	if(mpEnemy)
	{
		//Stop it where the player left it, so the AI does not inherit a sprint.
		iCharacterBody *pBody = mpEnemy->GetCharacterBody();
		if(pBody) pBody->StopMovement();

		mpEnemy->SetPossessed(false);

		////////////////////////////////
		// A morphed monster was made for us, so it goes with us.
		//
		// Only through the map that created it, and only while that map is still
		// the loaded one -- DestroyEntity queues onto that map's own to-destroy
		// list, and a map we have already left has torn its entities down anyway.
		// On that path the entity pointer is dead too, hence the map check before
		// anything is done with it.
		if(mlMorphType >= 0 && mpMorphMap != NULL && gpBase->mpMapHandler != NULL &&
			gpBase->mpMapHandler->GetCurrentMap() == mpMorphMap)
		{
			mpMorphMap->DestroyEntity(mpEnemy);
		}

		mpEnemy = NULL;
	}

	mlMorphType = -1;
	mpMorphMap = NULL;

	UnbindCamera();

	mfWishForward = 0;
	mfWishRight = 0;
	mbRunning = false;
}

//-----------------------------------------------------------------------

bool cLuxPlayerPossess::Morph(int alMorphType)
{
	if(::ImGuiDebugMenu::GetAllowEnemyMorph()==false) return false;
	if(mpPlayer->IsDead()) return false;

	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL) return false;

	iCharacterBody *pPlayerBody = mpPlayer->GetCharacterBody();
	if(pPlayerBody==NULL) return false;

	const cLuxMorphDef &def = gvMorphDefs[alMorphType];

	////////////////////////////////
	// Spawn it standing exactly where we are
	//
	// iLuxEnemyLoader::AfterLoad ends with SetFeetPosition(entity world position),
	// so the transform's translation IS the feet -- no half-height correction, and
	// nothing to shove out of a wall the way spawning a step ahead would need.
	const cVector3f vFeet = pPlayerBody->GetFeetPosition();
	const float fYaw = pPlayerBody->GetYaw();

	const tString sName = "morph_" + tString(def.msName) + "_" +
						  cString::ToString(glMorphSpawnCount++);

	pMap->ResetLatestEntity();
	pMap->CreateEntity(sName, def.msEntFile, cMath::MatrixTranslate(vFeet), cVector3f(1,1,1));

	iLuxEntity *pEntity = pMap->GetLatestEntity();
	if(pEntity==NULL || pEntity->GetEntityType() != eLuxEntityType_Enemy) return false;

	iLuxEnemy *pEnemy = static_cast<iLuxEnemy*>(pEntity);
	iCharacterBody *pEnemyBody = pEnemy->GetCharacterBody();
	if(pEnemyBody==NULL)
	{
		pMap->DestroyEntity(pEnemy);
		return false;
	}

	//////////////////////////////
	// Never write it to a save.
	//
	// cLuxSavedMap only records an entity that IsSaved() and is not already being
	// destroyed, so this one is skipped by every save path there is -- otherwise a
	// quicksave taken mid-morph would leave a live monster in the map forever, one
	// per morph, with its AI back on.
	pEnemy->SetIsSaved(false);

	//Face where we were facing. The loader only ever sets the position.
	pEnemyBody->SetYaw(fYaw);

	//////////////////////////////
	// Put our own body away.
	//
	// Possession parks Player 1's body and leaves it standing in the room, which is
	// right for possession -- that IS still you over there. Morphing is meant to be
	// you, so the body has to stop being in the world at all: two character bodies
	// spawned inside each other would otherwise shove each other apart on the first
	// frame, and Player 2 would see a Daniel stood next to the monster.
	//
	// SetActive(false) deactivates the underlying physics body and makes
	// iCharacterBody::Update early-out, so it neither collides nor falls. Unmorph
	// switches it back on and moves it to wherever the monster finished.
	pPlayerBody->SetForceVelocity(0);
	pPlayerBody->StopMovement();
	pPlayerBody->SetActive(false);

	//////////////////////////////
	// Take it over -- from here on it is an ordinary possession
	mpEnemy = pEnemy;
	mpEnemy->SetPossessed(true);

	mlMorphType = alMorphType;
	mpMorphMap = pMap;

	mfCamYaw = fYaw;
	mfCamPitch = -cMath::ToRad(12.0f);

	mfWishForward = 0;
	mfWishRight = 0;
	mbRunning = false;

	BindCamera();
	PoseCamera();

	gpBase->mpDebugHandler->AddMessage(
		cString::To16Char("Morphed into " + tString(def.msName)), false);

	return true;
}

//-----------------------------------------------------------------------

iLuxEnemy* cLuxPlayerPossess::PickEnemyUnderCrosshair()
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL) return NULL;

	iPhysicsWorld *pPhysicsWorld = pMap->GetPhysicsWorld();
	if(pPhysicsWorld==NULL) return NULL;

	cCamera *pCam = mpPlayer->GetCamera();
	if(pCam==NULL) return NULL;

	//The gun's callback, reused deliberately: it already skips both players and
	//keeps the nearest hit, and it exists precisely because it does NOT drop
	//characters the way cLuxMapHelper's own closest-entity callback does.
	//
	//GetInteractionForward, not the camera forward: in split screen the camera each
	//player aims with is not the one the renderer last touched.
	const cVector3f vStart = pCam->GetPosition();
	const cVector3f vDir = mpPlayer->GetInteractionForward();

	gGunRayCallback.Reset();
	pPhysicsWorld->CastRay(&gGunRayCallback, vStart, vStart + vDir*gfPossessRange,
						   true, false, false, true);

	iPhysicsBody *pBody = gGunRayCallback.mpClosestBody;
	if(pBody==NULL) return NULL;

	//Body -> entity, the way cLuxMapHelper::ShapeDamage does it. An enemy keeps its
	//entity pointer on the cCharacterBody (LuxEnemy.cpp: pCharBody->SetUserData(pEnemy)),
	//NOT on the physics body -- reading GetUserData() off the physics body returns
	//NULL for every monster in the game.
	iLuxEntity *pEntity = NULL;
	if(pBody->IsCharacter())	pEntity = (iLuxEntity*)pBody->GetCharacterBody()->GetUserData();
	else						pEntity = (iLuxEntity*)pBody->GetUserData();

	if(pEntity==NULL) return NULL;
	if(pEntity->GetEntityType() != eLuxEntityType_Enemy) return NULL;

	return static_cast<iLuxEnemy*>(pEntity);
}

//-----------------------------------------------------------------------

void cLuxPlayerPossess::BindCamera()
{
	cScene *pScene = gpBase->mpEngine->GetScene();

	if(mpCam==NULL)
	{
		mpCam = pScene->CreateCamera(eCameraMoveMode_Walk);

		//Same clip planes and FOV the player camera is built with, read from the
		//same config keys rather than copied off the player camera -- that one may
		//be mid-fade on mfFOVMul when possession starts.
		cVector2f vScreenSize = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();

		mpCam->SetFOV(cMath::ToRad(gpBase->mpGameCfg->GetFloat("Player_General","FOV", 0)));
		mpCam->SetAspect(vScreenSize.x / vScreenSize.y);
		mpCam->SetFarClipPlane(gpBase->mpGameCfg->GetFloat("Player_General","FarClipPlane",0));
		mpCam->SetNearClipPlane(gpBase->mpGameCfg->GetFloat("Player_General","NearClipPlane",0));
		mpCam->SetPitchLimits(-cMath::ToRad(70), cMath::ToRad(70));
	}

	//Player 1's half in split screen, the single viewport otherwise. Never P2's.
	cViewport *pViewport = gpBase->mpMapHandler->GetCoopMode()
						 ? gpBase->mpMapHandler->GetCoopP1Viewport()
						 : gpBase->mpMapHandler->GetViewport();

	if(pViewport==NULL) return;

	mpBoundViewport = pViewport;
	mpSavedCamera = pViewport->GetCamera();
	pViewport->SetCamera(mpCam);
}

//-----------------------------------------------------------------------

void cLuxPlayerPossess::UnbindCamera()
{
	if(mpBoundViewport)
	{
		//Only hand it back if we are still the one holding it. A map change nulls
		//the co-op viewport cameras and then re-points them at the players, so on
		//that path there is nothing of ours left to undo.
		if(mpSavedCamera && mpBoundViewport->GetCamera() == mpCam)
			mpBoundViewport->SetCamera(mpSavedCamera);

		mpBoundViewport = NULL;
	}

	mpSavedCamera = NULL;
}

//-----------------------------------------------------------------------

void cLuxPlayerPossess::PoseCamera()
{
	if(mpEnemy==NULL || mpCam==NULL) return;

	iCharacterBody *pBody = mpEnemy->GetCharacterBody();
	if(pBody==NULL) return;

	mpCam->SetYaw(mfCamYaw);
	mpCam->SetPitch(mfCamPitch);
	mpCam->SetRoll(0);

	//GetForward() goes through GetViewMatrix(), which rebuilds whenever yaw or pitch
	//changed -- so this is the direction just asked for, not last frame's. The
	//rotation part does not depend on the position, which is why reading it before
	//setting the position is safe.
	const cVector3f vFwd = mpCam->GetForward();

	cVector3f vFocus = pBody->GetPosition();
	vFocus.y += ::ImGuiDebugMenu::GetPossessCamHeight();

	float fDist = ::ImGuiDebugMenu::GetPossessCamDistance();

	//Pull in rather than sink through a wall.
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	iPhysicsWorld *pPhysicsWorld = pMap ? pMap->GetPhysicsWorld() : NULL;
	if(pPhysicsWorld && fDist > 0)
	{
		gPossessCamRayCallback.Reset();
		pPhysicsWorld->CastRay(&gPossessCamRayCallback, vFocus, vFocus - vFwd*fDist,
							   true, false, false, true);

		if(gPossessCamRayCallback.mfClosestDist >= 0)
		{
			fDist = gPossessCamRayCallback.mfClosestDist - 0.25f;
			if(fDist < 0.35f) fDist = 0.35f;
		}
	}

	mpCam->SetPosition(vFocus - vFwd*fDist);
}

//-----------------------------------------------------------------------
