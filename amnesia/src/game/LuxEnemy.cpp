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

#include "LuxEnemy.h"

#include "LuxEnemyMover.h"
#include "LuxEnemyPathfinder.h"

#include "LuxMap.h"
#include "LuxMapHelper.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"
#include "LuxInteractConnections.h"
#include "LuxMusicHandler.h"
#include "LuxEffectRenderer.h"
#include "LuxMoveState_Normal.h"
#include "LuxDebugHandler.h"
#include "LuxProgressLogHandler.h"
#include "LuxHelpFuncs.h"

#include "LuxProp.h"
#include "LuxProp_Object.h"

#include "impl/ImGuiDebugMenu.h"

#include "physics/PhysicsJointBall.h"

//////////////////////////////////////////////////////////////////////////
// Global Vars
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

tWString gsLuxEnemyStates[] =
{
	_W("Idle"),
	_W("GoHome"),

	_W("Wait"),
	_W("Patrol"),
	_W("Investigate"),
	_W("Alert"),
	_W("Search"),
	_W("Eat"),

	_W("Hurt"),
	
	_W("Hunt"),
	_W("HuntPause"),
	_W("HuntWander"),
	
	_W("AttackMeleeShort"),
	_W("AttackMeleeLong"),
	_W("AttackRange"),
	_W("BreakDoor"),
	
	_W("Dead"),
	
	_W("NULL"), // End of pre-Pig states
	
	_W("Flee"),
	_W("Stalk"),
	_W("Track"),
	
	_W("NULL")
};

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iLuxEnemyLoader::iLuxEnemyLoader(const tString& asName) : cEntityLoader_Object(asName)
{
	mfDefaultMaxFocusDistance = gpBase->mpGameCfg->GetFloat("Player_Interaction", "Default_DefaultMaxFocusDist",0);
}

//-----------------------------------------------------------------------

void iLuxEnemyLoader::BeforeLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{

}

//-----------------------------------------------------------------------

static eLuxEnemyPoseType ToPoseType(const tString& asPose)
{
	tString sLowStr = cString::ToLowerCase(asPose);

	if(sLowStr == "biped") return eLuxEnemyPoseType_Biped;
	if(sLowStr == "quadruped") return eLuxEnemyPoseType_Quadruped;

	Error("Pose type '%s' does not exist! Using biped\n", asPose.c_str());
	return eLuxEnemyPoseType_Biped;
}

//-----------------------------------------------------------------------

void iLuxEnemyLoader::AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
	cLuxMap *pMap = gpBase->mpCurrentMapLoading;
	if(pMap==NULL) return;
	
	iLuxEnemy *pEnemy = CreateEnemy(mpEntity->GetName(), mlID,pMap);

	//////////////////////////////
	// Setup mesh entity
	mpEntity->Stop();
	//mpEntity->SetNormalizeAnimationWeights(false);

	//////////////////////////////
	// Set data
	pEnemy->mpWorld = apWorld;
	pEnemy->msFileName = msFileName;
	pEnemy->m_mtxOnLoadTransform = a_mtxTransform;
	pEnemy->mvOnLoadScale = mvScale;

	pEnemy->mpMeshEntity = mpEntity;

	pEnemy->mvLights = mvLights;
	pEnemy->mvParticleSystems = mvParticleSystems;
	pEnemy->mvBillboards = mvBillboards;
	pEnemy->mvBeams = mvBeams;
	
	//////////////////////////////
	// Load base properties
	pEnemy->mfHealth = GetVarFloat("Health", 100);
	pEnemy->mlToughness = GetVarInt("Toughness", 0);

	pEnemy->mfSightRange = GetVarFloat("SightRange", 0);
	pEnemy->mfDarknessSightRange = GetVarFloat("DarknessSightRange", 0);
	pEnemy->mfHearVolume = GetVarFloat("HearVolume", 0);

	pEnemy->mfFOV = cMath::ToRad(GetVarFloat("FOV", 0));
	pEnemy->mfFOVXMul = GetVarFloat("FOVXMul", 1);

	pEnemy->mfMaxRegenHealth = GetVarFloat("MaxRegenHealth", 100);
	pEnemy->mfRegenHealthSpeed = GetVarFloat("RegenHealthSpeed", 0);

	pEnemy->mfActivationDistance = GetVarFloat("ActivationDistance", 0);

	pEnemy->msDangerMusic = GetVarString("DangerMusic", "");
	pEnemy->mlDangerMusicPrio = GetVarInt("DangerMusicPrio", 0);
	
	pEnemy->msMusic[eLuxEnemyMusic_Search] = GetVarString("SearchMusic", "");
	pEnemy->mlMusicPrio[eLuxEnemyMusic_Search] = GetVarInt("SearchMusicPrio", 0);

	pEnemy->msMusic[eLuxEnemyMusic_Attack] = GetVarString("AttackMusic", "");
	pEnemy->mlMusicPrio[eLuxEnemyMusic_Attack] = GetVarInt("AttackMusicPrio", 0);

	pEnemy->mbAutoRemoveAtPathEnd = GetVarBool("AutoRemoveAtPathEnd", true);

	//////////////////////////////
	// AI base properties
	pEnemy->mfPlayerSearchMaxAngle = cMath::ToRad(GetVarFloat("PlayerSearchMaxAngle", 0));
	pEnemy->mfPlayerSearchMinDistMul = GetVarFloat("PlayerSearchMinDistMul", 0);
	pEnemy->mfPlayerSearchMaxDistMul = GetVarFloat("PlayerSearchMaxDistMul", 0);
	pEnemy->mfPlayerSearchTime = GetVarFloat("PlayerSearchTime", 0);

	pEnemy->mfPlayerPatrolMaxAngle = cMath::ToRad(GetVarFloat("PlayerPatrolMaxAngle", 0));
	pEnemy->mfPlayerPatrolMinDist = GetVarFloat("PlayerPatrolMinDist", 0);
	pEnemy->mfPlayerPatrolMaxDist = GetVarFloat("PlayerPatrolMaxDist", 0);


    //////////////////////////////
	// Load character body
	iCharacterBody *pCharBody = apWorld->GetPhysicsWorld()->CreateCharacterBody(msName, GetVarVector3f("Body_Size", 1));
	
	pCharBody->SetMass(					GetVarFloat("Body_Mass", 1));
	pCharBody->SetAccurateClimbing(		GetVarBool("Body_AccurateClimbing",false) );
	pCharBody->SetMaxNoSlideSlopeAngle(cMath::ToRad(GetVarFloat("Body_MaxNoSlideSlopeAngle",0) ) );
	pCharBody->SetMaxPushMass(			GetVarFloat("Body_MaxPushMass",0) );
	pCharBody->SetPushForce(			GetVarFloat("Body_PushForce",0) );
	pCharBody->SetCharacterMaxPushMass(	GetVarFloat("Body_CharacterMaxPushMass",0) );
	pCharBody->SetCharacterPushForce(	GetVarFloat("Body_CharacterPushForce",0) );
	pCharBody->SetMaxStepSize(			GetVarFloat("Body_MaxStepSize",0) );
	pCharBody->SetMaxStepSizeInAir(		GetVarFloat("Body_MaxStepSize",0) );
	pCharBody->SetStepClimbSpeed(		GetVarFloat("Body_StepClimbSpeed",0) );

	pEnemy->m_mtxCharMeshOffset = cMath::MatrixRotate(  cMath::Vector3ToRad(GetVarVector3f("Body_OffsetRot", 0)), eEulerRotationOrder_XYZ);
	pEnemy->m_mtxCharMeshOffset.SetTranslation( GetVarVector3f("Body_OffsetTrans", 0)- cVector3f(0,pCharBody->GetSize().y/2,0) );

	pCharBody->SetEntity(pEnemy->mpMeshEntity);
	pCharBody->SetEntityOffset(pEnemy->m_mtxCharMeshOffset);
	pCharBody->SetUseEntitySmoothYPos(true);
	pCharBody->SetEntitySmoothYPosNum(20);

	pCharBody->SetFeetPosition(mpEntity->GetWorldPosition());

    pCharBody->SetUserData(pEnemy);
	pEnemy->mpCharBody = pCharBody;

	//////////////////////////////
	// Load default movement speeds
	pEnemy->mfDefaultForwardSpeed[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Walk] =	GetVarFloat("Walk_ForwardSpeed", 0);
	pEnemy->mfDefaultBackwardSpeed[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Walk] =	GetVarFloat("Walk_BackwardSpeed", 0);
	pEnemy->mfDefaultForwardAcc[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Walk] =		GetVarFloat("Walk_ForwardAcc", 0);
	pEnemy->mfDefaultForwardDeacc[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Walk] =	GetVarFloat("Walk_ForwardDeacc", 0);

	pEnemy->mfDefaultForwardSpeed[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Run] =	GetVarFloat("Run_ForwardSpeed", 0);
	pEnemy->mfDefaultBackwardSpeed[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Run] =	GetVarFloat("Run_BackwardSpeed", 0);
	pEnemy->mfDefaultForwardAcc[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Run] =		GetVarFloat("Run_ForwardAcc", 0);
	pEnemy->mfDefaultForwardDeacc[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Run] =	GetVarFloat("Run_ForwardDeacc", 0);

	pEnemy->mfDefaultForwardSpeed[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Walk] =	0;
	pEnemy->mfDefaultBackwardSpeed[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Walk] =	0;
	pEnemy->mfDefaultForwardAcc[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Walk] =		0;
	pEnemy->mfDefaultForwardDeacc[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Walk] =	0;

	pEnemy->mfDefaultForwardSpeed[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Run] =	0;
	pEnemy->mfDefaultBackwardSpeed[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Run] =	0;
	pEnemy->mfDefaultForwardAcc[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Run] =		0;
	pEnemy->mfDefaultForwardDeacc[eLuxEnemyPoseType_Quadruped][eLuxEnemyMoveSpeed_Run] =	0;


	//////////////////////////////
	// Load movement variables
	pEnemy->mfTurnSpeedMul = GetVarFloat("TurnSpeedMul", 1);
	pEnemy->mfTurnMaxSpeed = GetVarFloat("TurnMaxSpeed", 1);
	pEnemy->mfTurnMinBreakAngle = cMath::ToRad(GetVarFloat("TurnMinBreakAngle", 1));
	pEnemy->mfTurnBreakMul = GetVarFloat("TurnBreakMul", 1);

	pEnemy->mfMoveSpeedAnimMul =	GetVarFloat("MoveSpeedAnimMul", 1);

	pEnemy->mfStoppedToWalkSpeed[eLuxEnemyPoseType_Biped] =	GetVarFloat("StoppedToWalkSpeed", 0);
	pEnemy->mfWalkToStoppedSpeed[eLuxEnemyPoseType_Biped] =	GetVarFloat("WalkToStoppedSpeed", 0);
	pEnemy->mfWalkToRunSpeed[eLuxEnemyPoseType_Biped] =		GetVarFloat("WalkToRunSpeed", 0);
	pEnemy->mfRunToWalkSpeed[eLuxEnemyPoseType_Biped] =		GetVarFloat("RunToWalkSpeed", 0);

	pEnemy->mfStoppedToWalkSpeed[eLuxEnemyPoseType_Quadruped] =	0;
	pEnemy->mfWalkToStoppedSpeed[eLuxEnemyPoseType_Quadruped] = 0;
	pEnemy->mfWalkToRunSpeed[eLuxEnemyPoseType_Quadruped] =		0;
	pEnemy->mfRunToWalkSpeed[eLuxEnemyPoseType_Quadruped] =		0;

	pEnemy->mfWaterStepSpeedWalk = GetVarFloat("WaterStepSpeedWalk", 0);
	pEnemy->mfWaterStepSpeedRun = GetVarFloat("WaterStepSpeedRun", 0);
	pEnemy->mfWaterStepSpeedMisc = GetVarFloat("WaterStepSpeedMisc", 0);
	
	//////////////////////////////
	// Load hit variables
	pEnemy->msHitPS[eLuxWeaponHitType_Sword] = GetVarString("HitPS_Sword");
	pEnemy->msHitSound[eLuxWeaponHitType_Sword] = GetVarString("HitSound_Sword");

	pEnemy->msHitPS[eLuxWeaponHitType_Club] = GetVarString("HitPS_Club");
	pEnemy->msHitSound[eLuxWeaponHitType_Club] = GetVarString("HitSound_Club");

	pEnemy->msHitPS[eLuxWeaponHitType_Bullet] = GetVarString("HitPS_Club");
	pEnemy->msHitSound[eLuxWeaponHitType_Bullet] = GetVarString("HitSound_Club");
	
	pEnemy->msHitPS[eLuxWeaponHitType_Dud] = GetVarString("HitPS_Dud");
	pEnemy->msHitSound[eLuxWeaponHitType_Dud] = GetVarString("HitSound_Dud");

	//////////////////////////////
	// Load attack variables
	pEnemy->mfNormalAttackDistance = GetVarFloat("NormalAttackDistance", 0);

	//////////////////////////
	// HARDMODE 
	if (gpBase->mbHardMode)
	{
		pEnemy->mfSightRange *= 1.15f;
		pEnemy->mfDarknessSightRange *= 1.15f;
		pEnemy->mfHearVolume *= 0.6f;

		pEnemy->mfFOV *= 1.1f;
		pEnemy->mfFOVXMul *= 1.1f;

		pEnemy->mfPlayerSearchMaxAngle *= 1.1f;
		pEnemy->mfPlayerSearchMinDistMul *= 1.25f;
		pEnemy->mfPlayerSearchMaxDistMul *= 1.25f;
		
		pEnemy->mfPlayerSearchTime *= 2.5f;

		pEnemy->mfPlayerPatrolMaxAngle	*= 1.25f;
		pEnemy->mfPlayerPatrolMinDist	*= 1.5f;
		pEnemy->mfPlayerPatrolMaxDist	*= 1.5f;

		pEnemy->mfDefaultForwardSpeed[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Walk]		*= 1.1f;
		pEnemy->mfDefaultBackwardSpeed[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Walk]	*= 1.1f;
		pEnemy->mfDefaultForwardAcc[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Walk]		*= 1.1f;
		pEnemy->mfDefaultForwardDeacc[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Walk]		*= 1.1f;

		pEnemy->mfDefaultForwardSpeed[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Run]		*= 1.2f;
		pEnemy->mfDefaultBackwardSpeed[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Run]		*= 1.2f;
		pEnemy->mfDefaultForwardAcc[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Run]		*= 1.2f;
		pEnemy->mfDefaultForwardDeacc[eLuxEnemyPoseType_Biped][eLuxEnemyMoveSpeed_Run]		*= 1.2f;

		pEnemy->mfTurnSpeedMul		*= 1.35f;
		pEnemy->mfTurnMaxSpeed		*= 1.0f;
		pEnemy->mfTurnMinBreakAngle *= 1.0f;
		pEnemy->mfTurnBreakMul		*= 1.0f;

		pEnemy->mfStoppedToWalkSpeed[eLuxEnemyPoseType_Biped]	*= 1.0f;
		pEnemy->mfWalkToStoppedSpeed[eLuxEnemyPoseType_Biped]	*= 1.0f;
		pEnemy->mfWalkToRunSpeed[eLuxEnemyPoseType_Biped]		*= 1.0f;
		pEnemy->mfRunToWalkSpeed[eLuxEnemyPoseType_Biped]		*= 1.0f;


		pEnemy->mfWaterStepSpeedWalk	*= 1.5f;
		pEnemy->mfWaterStepSpeedRun		*= 1.5f;
		pEnemy->mfWaterStepSpeedMisc	*= 1.0f;
	}


	LoadAttackDamageData("Normal", &pEnemy->mNormalAttackDamage);
	LoadAttackDamageData("BreakDoor", &pEnemy->mBreakDoorAttackDamage);
	LoadAttackSizeData("Normal", &pEnemy->mNormalAttackSize, pEnemy, apWorld); 
	
	
	//////////////////////////////
	// Load ambient sounds
	pEnemy->msAmbientSound[eLuxEnemySoundState_Idle] = GetVarString("AmbientSound_Idle");
	pEnemy->mfAmbientSoundMinTime[eLuxEnemySoundState_Idle] = GetVarFloat("AmbientMinTime_Idle");
	pEnemy->mfAmbientSoundMaxTime[eLuxEnemySoundState_Idle] = GetVarFloat("AmbientMaxTime_Idle");

	pEnemy->msAmbientSound[eLuxEnemySoundState_Alert] = GetVarString("AmbientSound_Alert");
	pEnemy->mfAmbientSoundMinTime[eLuxEnemySoundState_Alert] = GetVarFloat("AmbientMinTime_Alert");
	pEnemy->mfAmbientSoundMaxTime[eLuxEnemySoundState_Alert] = GetVarFloat("AmbientMaxTime_Alert");

	pEnemy->msAmbientSound[eLuxEnemySoundState_Hunt] = GetVarString("AmbientSound_Hunt");
	pEnemy->mfAmbientSoundMinTime[eLuxEnemySoundState_Hunt] = GetVarFloat("AmbientMinTime_Hunt");
	pEnemy->mfAmbientSoundMaxTime[eLuxEnemySoundState_Hunt] = GetVarFloat("AmbientMaxTime_Hunt");

	for(size_t i=0; i<eLuxEnemySoundState_LastEnum; ++i)
		gpBase->PreloadSound(pEnemy->msAmbientSound[i]);
	
	//////////////////////////////
	// Load type specific Properties
	LoadVariables(pEnemy, apRootElem);

	//////////////////////////////
	// Extra setup

	pEnemy->SetupAfterLoad(apWorld);

	//Add to the current map
	pMap->AddEntity(pEnemy);

	pEnemy->SetActive(mbActive);

	///////////////////////////////
	// Instance vars
	if(apInstanceVars)
	{
		pEnemy->msCallbackFunc = apInstanceVars->GetVarString("CallbackFunc", "");
		pEnemy->mbDisableTriggers = apInstanceVars->GetVarBool("DisableTriggers", false);
		pEnemy->mbHallucination =  apInstanceVars->GetVarBool("Hallucination", false);
		pEnemy->mfHallucinationEndDist = apInstanceVars->GetVarFloat("HallucinationEndDist", false);

		pEnemy->mCurrentPose = ToPoseType(apInstanceVars->GetVarString("Pose", "biped"));

		LoadInstanceVariables(pEnemy, apInstanceVars);
	}
}

//-----------------------------------------------------------------------

eLuxDamageType iLuxEnemyLoader::ToDamageType(const tString& asType)
{
	tString sLowType = cString::ToLowerCase(asType);

	if(sLowType == "bloodsplat") return eLuxDamageType_BloodSplat;
	if(sLowType == "claws") return eLuxDamageType_Claws;
	if(sLowType == "slash") return eLuxDamageType_Slash;

	Error("Damage type '%s' does not exist!\n", asType.c_str());
	return eLuxDamageType_BloodSplat;
}

//-----------------------------------------------------------------------


void iLuxEnemyLoader::LoadAttackDamageData(const tString &asPrefix, cEnemyAttackDamageData *apData)
{
	cVector2f vDamageMinMax = GetVarVector2f(asPrefix+"AttackDamage", 0);
	cVector2f vForceAndMaxImpulse = GetVarVector2f(asPrefix+"AttackForce", 0);
	apData->msHitSound = GetVarString(asPrefix+"AttackHitSound", "");
	apData->mlStrength = GetVarInt(asPrefix+"AttackStrength", 0);
	apData->mfMinDamage = vDamageMinMax.x;
	apData->mfMaxDamage = vDamageMinMax.y;
	apData->mfForce = vForceAndMaxImpulse.x;
	apData->mfMaxImpulse = vForceAndMaxImpulse.y;
	apData->mDamageType =  ToDamageType(GetVarString(asPrefix+"DamageType","bloodsplat"));
	gpBase->PreloadSound(apData->msHitSound);

	///////////////////////
	// HARDMODE
	if (gpBase->mbHardMode)
	{
		// Double min & max damage
		apData->mfMinDamage *= 2.0f;
		apData->mfMaxDamage *= 2.0f;
	}
}

//-----------------------------------------------------------------------

void iLuxEnemyLoader::LoadAttackSizeData(const tString &asPrefix, cEnemyAttackSizeData *apData, iLuxEnemy *apEnemy, cWorld *apWorld)
{
	cVector3f vSize = GetVarVector3f(asPrefix+"DamageSize", 0);
	apData->mvOffset = GetVarVector3f(asPrefix+"DamageOffset", 0);
	apData->mlShapeIdx = apEnemy->CreateAttackShape(apWorld,vSize);
}


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iLuxEnemy::iLuxEnemy(const tString &asName, int alID, cLuxMap *apMap, eLuxEnemyType aEnemyType) : iLuxEntity(asName,alID,apMap, eLuxEntityType_Enemy)
{
	mEnemyType = aEnemyType;

	mbUseAnimations = true;

	mfInLanternLightCount =0;

	mbCausesSanityDecrease = true;
	mbCausesSanityDecreaseAsDefault = true;

	mbPossessed = false;
	mbSanityDecreaseBeforePossess = true;

	mbKilled = false;
	mbRagdollActive = false;
	mfPropHitCooldown = 0;

	mNextState = eLuxEnemyState_Idle;
	mCurrentState = eLuxEnemyState_LastEnum;
	mPreviousState = eLuxEnemyState_LastEnum;
	
	mSoundState = eLuxEnemySoundState_Silent;

	mpCurrentSound = NULL;
	mfAmbientSoundCount = 0;

	mfLookForPlayerCount =0;
	mbCanSeePlayer = false;
	mlPlayerInLOSCount =0;
	mbPlayerDetected = false;
	mbPlayerInRange = false;

	mfCheckAtDoorCount =0;
	mbStuckAtDoor = false;

	mfForwardSpeed = 1;
	mfBackwardSpeed =1;
	mfForwardAcc =1;
	mfForwardDeacc =1;

	mfDarknessGlowAlpha =0;
	mfDarknessGlowUpdateCount =0;

	mfFOVMul = 1.0f;

	mpCurrentAnimation = NULL;

	mbAnimationIsSpeedDependant = false;
	mfAnimationSpeedMul = 1;
	mbUseMoveAnimWhenCurrentIsOver = true;

	mpLastSearchNode = NULL;

	mfDamageCount =0;

	mvLastKnownPlayerPos =0;
	mfLastPlayerPosCount =0;

	mpTargetPlayer = NULL;
	mfRetargetCount =0;

	mlCurrentPatrolNode = 0;

	mlAttackHitCounter =0;

	mbIsSeenByPlayer = false;

	mbDisabled = false;

	mbDisableTriggers = false;

	mbHallucination = false;
	mfHallucinationEndDist = 3.0f;
	
	mfEnemyDarknessGlowMaxDistance = gpBase->mpGameCfg->GetFloat("Enemy", "EnemyDarknessGlowMaxDistance",0);

	mfPlayerInDarknessLightLevel = 0.35f;
	mfCrouchVisibleRangeMul = 0.5f;

	m_mtxCharMeshOffset = cMatrixf::Identity;

	mfCheckGroundRayCount =0;
	mlMaxGroundCheckDists = 3;
	mbAlignEntityWithGroundRay = false;

	mbInWater = false;
	mfWaterSurfaceY = 0;
	mpWaterSurfaceData = NULL;

	mbSkipVisibilityRangeHandicaps = false;

	mCurrentPose = eLuxEnemyPoseType_Biped;

	for(int i=0; i<eLuxEnemyPoseType_LastEnum; ++i)
	for(int j=0; j<eLuxEnemyMoveType_LastEnum; ++j)
	{
		msBackwardAnimationName[j][i] = "Backward";
		msIdleAnimationName[j][i] = "Idle";
		msWalkAnimationName[j][i] = "Walk";
		msRunAnimationName[j][i] = "Run";
	}
}

//-----------------------------------------------------------------------

iLuxEnemy::~iLuxEnemy()
{
	cWorld *pWorld = mpMap->GetWorld();
	iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();

	
	////////////////////
	// Destroy graphics
	{
		//Mesh entity
		if(mpMeshEntity) pWorld->DestroyMeshEntity(mpMeshEntity);

		//Lights
		for(size_t i=0; i<mvLights.size(); ++i) pWorld->DestroyLight(mvLights[i]);
		
		//Particle systems
		for(size_t i=0; i<mvParticleSystems.size(); ++i) 
		{
			cParticleSystem *pPS = mvParticleSystems[i];
			if(pPS && pWorld->ParticleSystemExists(pPS)) pPS->Kill();
		}

		//Billboards
		for(size_t i=0; i<mvBillboards.size(); ++i)	pWorld->DestroyBillboard(mvBillboards[i]);
		
		//Beams
		for(size_t i=0; i<mvBeams.size(); ++i)		pWorld->DestroyBeam(mvBeams[i]);
	}

	////////////////////
	// Destroy attack shapes
	for(size_t i=0; i<mvAttackShapes.size();++i)
	{
		pPhysicsWorld->DestroyShape(mvAttackShapes[i]);
	}

	////////////////////
	// Physics
	if(mpCharBody)
	{
		pPhysicsWorld->DestroyCharacterBody(mpCharBody);
	}

	////////////////////
	// Destroy Helpers
	hplDelete(mpPathfinder);
	hplDelete(mpMover);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void iLuxEnemy::SetupAfterLoad(cWorld *apWorld)
{
	///////////////////////
	// Create helpers
	mpMover = hplNew(cLuxEnemyMover, (this,mpCharBody));
	mpPathfinder = hplNew(cLuxEnemyPathfinder, (this,mpMover));
	
	///////////////////////
	// Setup implemented 
	mpPathfinder->SetupAfterLoad(apWorld);
	mpMover->SetupAfterLoad(apWorld);

	///////////////////////
	// Setup implemented 
	OnSetupAfterLoad(apWorld);

	///////////////////////
	// Setup default move speeds
	SetMoveSpeed(eLuxEnemyMoveSpeed_Walk);
}

//-----------------------------------------------------------------------

void iLuxEnemy::AfterWorldLoad()
{
	mpPathfinder->AfterWorldLoad();

	OnAfterWorldLoad();
}

//-----------------------------------------------------------------------

void iLuxEnemy::OnMapEnter()
{
	mvStartPosition = mpCharBody->GetFeetPosition() + cVector3f(0,0.1f, 0);
}

//-----------------------------------------------------------------------

void iLuxEnemy::OnUpdate(float afTimeStep)
{
	if(mbDisabled) return;

	//In case the update is called in save or anyhting else that just want a quick update, we do not want to be updated.
	//Enemies can be disabled when player is not properly placed and the like.
	if(afTimeStep < gpBase->mpEngine->GetStepSize()*0.8f)
	{
		return;
	}

	//////////////////////
	// Helpers
	//
	// POSSESSION: the pathfinder is the AI's hands. cLuxEnemyPathfinder::UpdateMoving
	// picks the next path node and calls mpMover->MoveToPos every frame, so while a
	// player is steering it would fight them for the wheel and keep posting
	// eLuxEnemyMessage_EndOfPath into a message list nobody is draining for it.
	//
	// The MOVER always runs. Turning, the walk/run animation pick and the footstep
	// effects all live there, and it is purely a slave to whoever last called
	// MoveToPos / TurnToAngle. That is what makes a possessed enemy move like the
	// real thing instead of like a puppet.
	if(mbPossessed==false) mpPathfinder->OnUpdate(afTimeStep);
	mpMover->OnUpdate(afTimeStep);

	//////////////////////
	// Dead on load
	//
	// A save brings a corpse back at zero health and in eLuxEnemyState_Dead, but
	// STANDING: the state is copied straight into mCurrentState, so ChangeState
	// never re-enters and the Dead state's kLuxOnEnter never runs. Nothing about
	// the ragdoll is saved either -- the bodies are built at runtime and the pose
	// would mean versioning the save format.
	//
	// So collapse it again. It lands where it died, since the position is saved;
	// the exact heap differs, because it falls from the loaded pose rather than
	// resuming the one you left.
	//
	// Here rather than in SetupSaveData because StartRagdoll needs the mesh
	// entity and the character body, and the first update is unambiguously after
	// both exist. mbKilled makes it one bool compare on every other frame.
	if(mbKilled==false && mfHealth <= 0 && ::ImGuiDebugMenu::GetAllowKillingMonsters())
		OnKilled();

	//////////////////////
	// Prop hit rate limit
	if(mfPropHitCooldown > 0) mfPropHitCooldown -= afTimeStep;

	//////////////////////
	// Getting hit by thrown objects
	UpdatePropHits(afTimeStep);

	//////////////////////
	// Senses
	//
	// POSSESSION: all of it off, and the else branch below keeps the flags clear.
	// These are what tell the state machine to go hunting, and UpdatePlayerInRange
	// additionally feeds the ambient danger track -- cLuxMusicHandler::UpdateDangerMusic
	// reads GetPlayerInRange() directly, which no other suppression flag covers.
	if(mfHealth > 0 && mbPossessed==false)
	{
		// Coop: decide which player this enemy focuses on
		UpdateTargetPlayer(afTimeStep);

		// Sight
		UpdateCanSeePlayer(afTimeStep);
		
		// Detection
		UpdatePlayerDetected(afTimeStep);

		// In Range
		UpdatePlayerInRange(afTimeStep);

		// Is stuck at door check
		UpdateCheckStuckAtDoor(afTimeStep);

		// Check last player position
		UpdateCheckLastPlayerPos(afTimeStep);
	}
	else
	{
		mbCanSeePlayer = false;
		mbPlayerDetected = false;
		mbPlayerInRange = false;
	}
	
	//////////////////////
	// State update
	UpdateStateMachine(afTimeStep);
	
	//////////////////////
	// Specific
	UpdateEnemySpecific(afTimeStep);

	//////////////////////
	// Character body
	UpdateCharBody(afTimeStep);

	//////////////////////
	// Animation
	//A ragdolled corpse is posed by the solver. This function sends
	//eLuxEnemyMessage_AnimationOver and can call PlayAnim again, and either one
	//puts an animation state back to active -- which would bury the ragdoll under
	//a walk cycle all over again.
	if(mbRagdollActive==false) UpdateAnimation(afTimeStep);

	//////////////////////
	// Sound
	UpdateSoundState(afTimeStep);

	//////////////////////
	// Glow
	//A corpse should not still be glowing at whoever is looking at it.
	if(mbKilled==false) UpdateDarknessGlow(afTimeStep);

	//////////////////////
	// Health Regeneration
	UpdateRegenHealth(afTimeStep);

	//////////////////////
	// Hallucination
	UpdateHallucination(afTimeStep);

	//////////////////////
	// Align with ground ray
	UpdateAlignEntityWithGroundRay(afTimeStep);

	//////////////////////
	// Debug
	/*Log("Animations\n------------------\n");
	for(int i=0; i<mpMeshEntity->GetAnimationStateNum(); ++i)
	{
		cAnimationState *pState = mpMeshEntity->GetAnimationState(i);
		if(pState->GetWeight() == 0 || pState->IsActive()==false) continue;
		Log("  %s \tt: %f w:%f\n", pState->GetName().c_str(), pState->GetRelativeTimePosition(), pState->GetWeight());
	}*/
}

//-----------------------------------------------------------------------

void iLuxEnemy::OnRenderSolid(cRendererCallbackFunctions* apFunctions)
{
	//return;
	iPhysicsWorld *pPhysicsWorld = mpMap->GetPhysicsWorld();

	//pPhysicsWorld->RenderShapeDebugGeometry(mpCharBody->GetCurrentShape(), mpCharBody->GetMoveMatrix(), apFunctions->GetLowLevelGfx(),cColor(1,1));

	mpPathfinder->OnRenderSolid(apFunctions);

	apFunctions->GetLowLevelGfx()->DrawSphere(mvLastKnownPlayerPos, 0.3f, cColor(1,0,0));


	////////////////////////////////////////
	// See max range (against the current target player)
	cLuxPlayer *pPlayer = GetTargetPlayer();

	float fMaxRange = CalcPlayerSeeRange(pPlayer);

	apFunctions->GetLowLevelGfx()->DrawSphere(mpCharBody->GetPosition(), fMaxRange, cColor(0,1,1));

	OnRenderSolidImplemented(apFunctions);
}

//-----------------------------------------------------------------------

bool iLuxEnemy::CanInteract(iPhysicsBody *apBody)
{
	return false;
}

bool iLuxEnemy::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	return false;
}

//-----------------------------------------------------------------------

void iLuxEnemy::OnConnectionStateChange(iLuxEntity *apEntity, int alState)
{

}

//-----------------------------------------------------------------------

eLuxFocusCrosshair iLuxEnemy::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	return eLuxFocusCrosshair_LastEnum;
}

//-----------------------------------------------------------------------

iEntity3D* iLuxEnemy::GetAttachEntity()
{
	return mpMeshEntity;
}

//-----------------------------------------------------------------------

bool iLuxEnemy::mbIgnoreDamageResistance = false;

//-----------------------------------------------------------------------

void iLuxEnemy::GiveDamage(float afAmount, int alStrength)
{
	//Already dead and shut down. Without this, hitting a corpse would re-run
	//the death branch below and fire the OnDeath script callback again.
	//Inert when monster killing is disabled, because mbKilled is only ever set
	//by OnKilled and OnKilled only runs behind that switch.
	if(mbKilled) return;

	if(mfHealth > 0)

	if(alStrength < mlToughness-1)			afAmount =0;
	else if(alStrength == mlToughness-1)	afAmount *= 0.5;

	//The debug gun turns this off for its own shot -- see
	//SetIgnoreDamageResistance. Everything else still pays the state penalty.
	if(mbIgnoreDamageResistance==false)
		afAmount *= GetDamageMul(afAmount, alStrength);
	
	mfHealth -= afAmount;

	gpBase->mpDebugHandler->AddMessage(_W("Enemy damage ") + cString::ToStringW(afAmount), false);

	if(mfHealth <=0)
	{
		ChangeSoundState(eLuxEnemySoundState_Silent);
		ChangeState(eLuxEnemyState_Dead);

		RunCallbackFunc("OnDeath");

		//After the state change, so it can override what the per-enemy Dead state
		//just did -- that state runs synchronously inside ChangeState.
		if(::ImGuiDebugMenu::GetAllowKillingMonsters())
			OnKilled();
	}
	else
	{
		SendMessage(eLuxEnemyMessage_TakeHit,0,false,0,afAmount,alStrength);
		OnDamage(afAmount, alStrength);
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdatePropHits(float afTimeStep)
{
	if(::ImGuiDebugMenu::GetAllowKillingMonsters()==false) return;

	if(mbKilled) return;
	if(mfHealth <= 0) return;
	if(mfPropHitCooldown > 0) return;
	if(mpCharBody==NULL || mpMap==NULL) return;

	iPhysicsBody *pCharBody = mpCharBody->GetCurrentBody();
	if(pCharBody==NULL) return;

	cWorld *pWorld = mpMap->GetWorld();
	if(pWorld==NULL) return;

	iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();
	if(pPhysicsWorld==NULL) return;

	//Enough that carrying a crate into a monster, or one settling against it,
	//does not count. Any real throw clears it easily.
	const float kMinHitSpeed = 2.5f;

	cBoundingVolume *pCharBV = pCharBody->GetBoundingVolume();
	if(pCharBV==NULL) return;

	std::vector<iPhysicsBody*> vBodies;
	pPhysicsWorld->GetBodiesInBV(pCharBV, &vBodies);

	cVector3f vEnemyPos = mpCharBody->GetPosition();

	for(size_t i=0; i<vBodies.size(); ++i)
	{
		iPhysicsBody *pBody = vBodies[i];
		if(pBody==NULL || pBody==pCharBody) continue;

		//Massless means level geometry or a kinematic prop: it is not something
		//that was thrown, and it has no weight to convert into damage either.
		if(pBody->GetMass() <= 0) continue;

		//Not the players, not other monsters, and not another monster's corpse.
		if(pBody->IsCharacter()) continue;
		if(pBody->IsRagDoll()) continue;

		cVector3f vVel = pBody->GetLinearVelocity();
		if(vVel.Length() < kMinHitSpeed) continue;

		//Moving TOWARDS the enemy. This is what separates the frame a throw
		//connects from the frames afterwards, while the prop is still inside the
		//enemy's box on its way back out -- otherwise one throw reads as several.
		if(cMath::Vector3Dot(vVel, vEnemyPos - pBody->GetWorldPosition()) <= 0) continue;

		float fDamage = ::ImGuiDebugMenu::GetOneHitPropKill()
							? 1000000.0f
							: pBody->GetMass() * ::ImGuiDebugMenu::GetPropDamagePerKg();

		//Strength 100 clears every mlToughness tier. Toughness grades WEAPONS;
		//a crate to the head is not a weapon tier, so let mass decide instead.
		if(TakePropHit(fDamage, 100)) break;
	}
}

//-----------------------------------------------------------------------

bool iLuxEnemy::TakePropHit(float afDamage, int alStrength)
{
	if(mbKilled) return false;
	if(mfPropHitCooldown > 0) return false;

	mfPropHitCooldown = 0.35f;

	GiveDamage(afDamage, alStrength);

	//Flinch.
	//
	//GiveDamage already sent eLuxEnemyMessage_TakeHit, but nothing reliable comes
	//of it for a thrown crate:
	//  Grunt  accumulates into mfDamageCount and only flinches past
	//         mfGroggyDamageCount -- and what it accumulates has already been
	//         through GetDamageMul, which returns 0.2 for a hunting enemy. Exactly
	//         when you want the flinch, it counts a fifth.
	//  ManPig the handler body is COMMENTED OUT in the shipped source, so a brute
	//         never flinches from it at all.
	//  Lurker deliberately does nothing, and has no Hurt state to enter.
	//
	//So ask for it here. Still the enemy's own state and its own animation -- Hurt
	//stops the pathfinder, plays "Flinch" and leaves on AnimationOver into Hunt or
	//Alert. All this decides is when to go in.
	//
	//No stun lock: ChangeState early-outs while already in the state, so a second
	//crate mid-flinch cannot restart the animation. It plays out first.
	//
	//Only reachable from UpdatePropHits, which returns immediately unless Allow
	//Killing Monsters is on -- so that gate covers this too.
	if(mfHealth > 0 && HasFlinchState())
		ChangeState(eLuxEnemyState_Hurt);

	return true;
}

//-----------------------------------------------------------------------

void iLuxEnemy::OnKilled()
{
	if(mbKilled) return;
	mbKilled = true;

	////////////////////////////////
	// Stop haunting the players
	//
	// The per-enemy Dead states already drop terror and the Attack music, but
	// they leave the Search music registered -- only OnSetActive ever removes
	// that one -- and they leave mbCausesSanityDecrease alone. The sanity loop in
	// cLuxPlayerHelper_Sanity skips an enemy on IsActive()==false or
	// CausesSanityDecrease()==false, and a dead enemy is still active, so without
	// this the corpse goes on draining sanity from across the room.
	ChangeSoundState(eLuxEnemySoundState_Silent);

	SetSanityDecreaseActive(false);
	SetIsSeenByPlayer(false);

	RemoveTerrorFromAllPlayers();
	gpBase->mpPlayer->StopTerrorSound();
	if(gpBase->mpPlayer2) gpBase->mpPlayer2->StopTerrorSound();

	for(int i=0; i<eLuxEnemyMusic_LastEnum; ++i)
		gpBase->mpMusicHandler->RemoveEnemy((eLuxEnemyMusic)i, this);

	if(mpPathfinder) mpPathfinder->Stop();

	mbCanSeePlayer = false;
	mbPlayerDetected = false;
	mbPlayerInRange = false;

	////////////////////////////////
	// Collapse
	mbRagdollActive = StartRagdoll();
}

//-----------------------------------------------------------------------

bool iLuxEnemy::BuildRagdollBodies()
{
	if(mpMeshEntity == NULL || mpMap == NULL) return false;

	cWorld *pWorld = mpMap->GetWorld();
	if(pWorld==NULL) return false;

	iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();
	if(pPhysicsWorld==NULL) return false;

	int lBoneNum = mpMeshEntity->GetBoneStateNum();
	if(lBoneNum <= 0) return false;

	cNode3D *pBoneRoot = mpMeshEntity->GetBoneStateRoot();

	//Bones closer together than this are fingers, toes and helper bones. Giving
	//them their own body triples the joint count for something nobody can see.
	const float kMinBoneLength = 0.07f;

	////////////////////////////////
	// Pass 1: a body per bone
	int lCreated = 0;

	for(int i=0; i<lBoneNum; ++i)
	{
		cBoneState *pState = mpMeshEntity->GetBoneState(i);
		if(pState==NULL) continue;
		if(pState->GetBody()) { ++lCreated; continue; }

		cNode3D *pParentNode = pState->GetParent();
		cBoneState *pParent = (pParentNode && pParentNode != pBoneRoot)
								? static_cast<cBoneState*>(pParentNode) : NULL;

		//Size the body from the bone it hangs off. The root bone has nothing to
		//measure against, so it gets a torso-ish default.
		float fLength = 0.22f;
		if(pParent)
		{
			fLength = cMath::Vector3Dist(pState->GetWorldPosition(), pParent->GetWorldPosition());
			if(fLength < kMinBoneLength) continue;
		}

		//0.35 of the spacing: adjacent spheres touch but do not overlap, so the
		//solver does not begin by pushing the corpse apart at every joint.
		float fRadius = fLength * 0.35f;
		if(fRadius < 0.045f) fRadius = 0.045f;
		if(fRadius > 0.17f)  fRadius = 0.17f;

		iCollideShape *pShape = pPhysicsWorld->CreateSphereShape(cVector3f(fRadius), NULL);
		if(pShape==NULL) continue;

		iPhysicsBody *pBody = pPhysicsWorld->CreateBody(
									GetName() + "_ragdoll_" + pState->GetName(), pShape);
		if(pBody==NULL) continue;

		float fMass = fRadius * 45.0f;
		if(fMass < 0.8f) fMass = 0.8f;
		pBody->SetMass(fMass);

		//Sphere, so no orientation to get wrong: the body simply IS the bone, and
		//the bone-to-body offset below is the identity.
		pBody->SetMatrix(pState->GetWorldMatrix());

		pBody->SetIsRagDoll(true);
		//Ragdoll vs ragdoll OFF, ragdoll vs world ON -- see the note at the top of
		//this change. Every other body in the game defaults CollideRagDoll to true.
		pBody->SetCollideRagDoll(false);
		//A corpse is walked over, not used to shove the players around.
		pBody->SetCollideCharacter(false);
		pBody->SetCollide(true);
		pBody->SetGravity(true);
		pBody->SetAutoDisable(true);
		pBody->SetContinuousCollision(true);

		pState->SetBody(pBody);
		pState->SetBodyMatrix(cMatrixf::Identity);

		++lCreated;
	}

	if(lCreated == 0) return false;

	////////////////////////////////
	// Restored corpse: pose the bodies BEFORE the joints exist
	//
	// A Newton ball joint binds its pivot relative to both bodies at the moment
	// it is created. Build the skeleton standing and move it afterwards and every
	// joint hauls its limb back to where it was made -- the corpse snaps itself
	// into the air. Placing the bodies here means pass 2 measures the pivots off
	// the pose we actually want.
	bool bPosed = ((int)mvSavedRagdollPose.size() == lBoneNum);

	if(bPosed)
	{
		for(int i=0; i<lBoneNum; ++i)
		{
			cBoneState *pState = mpMeshEntity->GetBoneState(i);
			if(pState==NULL || pState->GetBody()==NULL) continue;

			pState->GetBody()->SetMatrix(mvSavedRagdollPose[i]);
		}
	}

	////////////////////////////////
	// Pass 2: hold it together
	//
	// Joined to the nearest ANCESTOR that got a body, not to the direct parent --
	// the small bones skipped above would otherwise break the chain and leave a
	// forearm falling on its own.
	for(int i=0; i<lBoneNum; ++i)
	{
		cBoneState *pState = mpMeshEntity->GetBoneState(i);
		if(pState==NULL || pState->GetBody()==NULL) continue;

		cNode3D *pNode = pState->GetParent();
		cBoneState *pAnchor = NULL;
		while(pNode && pNode != pBoneRoot)
		{
			cBoneState *pCandidate = static_cast<cBoneState*>(pNode);
			if(pCandidate->GetBody()) { pAnchor = pCandidate; break; }
			pNode = pCandidate->GetParent();
		}
		if(pAnchor==NULL) continue;

		//Off the BODIES, not the bones. On a fresh kill they sit on each other so
		//it makes no difference; on a restored corpse the bones are still standing
		//in the bind pose and only the bodies know where the limbs went.
		cVector3f vPivot = pState->GetBody()->GetWorldPosition();
		cVector3f vPin = vPivot - pAnchor->GetBody()->GetWorldPosition();
		if(vPin.Length() < 0.0001f) vPin = cVector3f(0,1,0);
		vPin.Normalize();

		iPhysicsJointBall *pJoint = pPhysicsWorld->CreateJointBall(
										GetName() + "_ragjoint_" + pState->GetName(),
										vPivot, vPin,
										pAnchor->GetBody(), pState->GetBody());

		//Without limits every joint is a free swivel and the corpse folds through
		//itself. Loose enough to still read as dead weight.
		if(pJoint) pJoint->SetConeLimits(cMath::ToRad(55.0f), cMath::ToRad(25.0f));
	}

	Log("Built runtime ragdoll for '%s': %d bone bodies.\n", GetName().c_str(), lCreated);

	return true;
}

//-----------------------------------------------------------------------

bool iLuxEnemy::StartRagdoll()
{
	if(mpMeshEntity == NULL) return false;

	////////////////////////////////
	// Does this model have a ragdoll to run?
	//
	// The bodies are DATA, not code: cEntityLoader_Object::AttachBoneToBody only
	// fills cBoneState::SetBody for bodies whose .ent lists a bone as a child.
	// SetSkeletonPhysicsActive on a model without them is a silent no-op that
	// leaves the corpse frozen in its death pose, so say what happened instead.
	int lBoneBodies = 0;
	for(int i=0; i<mpMeshEntity->GetBoneStateNum(); ++i)
	{
		if(mpMeshEntity->GetBoneState(i)->GetBody()) ++lBoneBodies;
	}

	if(lBoneBodies == 0)
	{
		//No authored ragdoll in the .ent -- which is every monster in the game.
		//Build one now rather than leaving the corpse frozen mid-animation.
		if(BuildRagdollBodies()==false)
		{
			Warning("Enemy '%s' has no usable skeleton, so it cannot ragdoll.\n",
					GetName().c_str());
			return false;
		}
	}

	////////////////////////////////
	// Seed the bodies from the pose it died in
	//
	// Do this BEFORE switching skeleton physics on. Afterwards the bones are
	// driven by the bodies, so reading the bones to place the bodies would be
	// reading back whatever the bodies already had.
	//
	// Skipped for a corpse restored from a save: BuildRagdollBodies has already
	// put the bodies where they came to rest, and this would push them back onto
	// bones that are still standing in the bind pose.
	bool bRestored = mvSavedRagdollPose.empty()==false;
	mvSavedRagdollPose.clear();

	if(bRestored==false) mpMeshEntity->AlignBodiesToSkeleton(false);

	////////////////////////////////
	// Make each bone body a real dynamic body
	//
	// While the monster is alive these are driven by the animation, so a model
	// may well author them massless. A massless body is kinematic: it would hang
	// in the air instead of falling.
	const float kBoneMassIfUnset = 6.0f;

	for(int i=0; i<mpMeshEntity->GetBoneStateNum(); ++i)
	{
		iPhysicsBody *pBody = mpMeshEntity->GetBoneState(i)->GetBody();
		if(pBody==NULL) continue;

		if(pBody->GetMass() <= 0) pBody->SetMass(kBoneMassIfUnset);

		//This is the answer to "does it still collide with the world": these are
		//ordinary bodies in the ordinary physics world, so level geometry and props
		//work with no extra machinery.
		//
		//CollideRagDoll(false) is ragdoll-vs-RAGDOLL only. The Newton gate is
		//`A->IsRagDoll() && B->GetCollideRagDoll()==false`, and every wall, floor and
		//prop defaults that flag to TRUE (iPhysicsBody's constructor; the .ent loader
		//never writes it) -- so world contacts survive and only the corpse's own
		//limbs stop fighting each other. Same line cEntityLoader_Object has commented
		//out directly under its SetIsRagDoll call.
		pBody->SetCollide(true);
		pBody->SetIsRagDoll(true);
		pBody->SetCollideRagDoll(false);
		//A corpse is walked over, not used to shove the players around the room.
		pBody->SetCollideCharacter(false);
		pBody->SetGravity(true);
		pBody->SetAutoDisable(true);

		//SetSkeletonPhysicsActive Enable()s every one of these in a moment. A
		//restored corpse would otherwise wake up carrying whatever velocity the
		//body happened to be created with.
		pBody->SetLinearVelocity(0);
		pBody->SetAngularVelocity(0);
	}

	////////////////////////////////
	// Silence the animation
	//
	// Not optional. cMeshEntity::UpdateLogic writes the bones from the bodies
	// FIRST and applies animation tracks to those same bones afterwards, gated
	// only on the animation state being active -- so anything still playing
	// overwrites the whole ragdoll pose every frame. The bodies fall, collide and
	// settle correctly underneath and not one frame of it reaches the screen.
	//
	// Nothing else would stop it: the Dead state asks for an animation the mesh
	// does not contain, so PlayAnim fails and the previous clip keeps looping.
	//
	// Stop() deactivates every animation state, which is exactly the condition
	// the track loop tests.
	mpMeshEntity->Stop();
	mpCurrentAnimation = NULL;

	////////////////////////////////
	// Hand the bones over
	//
	// This activates and enables every bone body itself and flips the collider
	// bodies off, so there is nothing to do afterwards. From here the bones
	// follow the bodies and nothing writes over them.
	mpMeshEntity->SetSkeletonPhysicsActive(true);

	////////////////////////////////
	// The walking capsule must let go
	//
	// Every Dead state already does this, but the ragdoll depends on it -- leave
	// it active and an invisible capsule holds the corpse upright and shoves it
	// around. Do not rely on each enemy type remembering.
	if(mpCharBody) mpCharBody->SetActive(false);

	return true;
}

//-----------------------------------------------------------------------

void iLuxEnemy::ShowPlayerPosition()
{
	mvLastKnownPlayerPos = GetTargetPlayer()->GetCharacterBody()->GetFeetPosition();
}

void iLuxEnemy::AlertOfPlayerPresence()
{
	if(TriggersDisabled()) return;

	ShowPlayerPosition();
	if(	mCurrentState == eLuxEnemyState_Idle ||	mCurrentState == eLuxEnemyState_Patrol)
	{
		ChangeState(eLuxEnemyState_Search);
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::ChangeState(eLuxEnemyState aState)
{
	if(aState == eLuxEnemyState_LastEnum) return;
	if(mCurrentState == aState) return;

	// POSSESSION: only states a player can be held responsible for.
	//
	// The senses are off, so nothing SHOULD ask for Hunt or Alert -- but the states
	// themselves still do. BreakDoor's _AnimationOver handler goes to Alert whenever
	// PlayerIsDetected(), and Alert's kLuxOnEnter re-adds terror and search music
	// behind our back. Anything off this list becomes Idle, which is where
	// possession parks between actions.
	if(mbPossessed)
	{
		switch(aState)
		{
		case eLuxEnemyState_Idle:
		case eLuxEnemyState_AttackMeleeShort:
		case eLuxEnemyState_AttackMeleeLong:
		case eLuxEnemyState_BreakDoor:
		case eLuxEnemyState_Hurt:
		case eLuxEnemyState_Dead:
			break;
		default:
			aState = eLuxEnemyState_Idle;
			if(mCurrentState == aState) return;
			break;
		}
	}

	//Log("State %s' -> '%s' LOS: %d\n", cString::To8Char(gsLuxEnemyStates[mCurrentState]).c_str(), cString::To8Char(gsLuxEnemyStates[aState]).c_str(),
	//									mbCanSeePlayer);
	
	mNextState = aState;
}

//-----------------------------------------------------------------------

void iLuxEnemy::SetPossessed(bool abX)
{
	if(mbPossessed == abX) return;

	mbPossessed = abX;

	if(abX)
	{
		//Let go of whatever path the AI was walking. This is the single call that
		//actually hands the wheel over -- see the note in OnUpdate.
		mpPathfinder->Stop();
		mpMover->ResetStuckCounter();

		//Senses cleared by hand rather than through the Update* functions, because
		//those send PlayerUnseen / PlayerUndetected / PlayerOutOfRange -- and
		//PlayerOutOfRange survives even the TriggersDisabled filter in SendMessage.
		//mbPlayerInRange in particular is what the ambient danger music reads.
		mbCanSeePlayer = false;
		mbPlayerDetected = false;
		mbPlayerInRange = false;
		mbStuckAtDoor = false;

		//Chase and search music. The handler keeps a std::set per music type, so
		//removing is exact and idempotent; its own 1.2s debounce does the fade.
		gpBase->mpMusicHandler->RemoveEnemy(eLuxEnemyMusic_Attack, this);
		gpBase->mpMusicHandler->RemoveEnemy(eLuxEnemyMusic_Search, this);

		//Terror is a std::set per player and cLuxPlayer::UpdateTerror only tests it
		//for emptiness, so one removal is enough. The heartbeat loop is a separate
		//sound that only stops once terror decays to zero, so stop it outright --
		//exactly what OnKilled does.
		RemoveTerrorFromAllPlayers();
		if(gpBase->mpPlayer)  gpBase->mpPlayer->StopTerrorSound();
		if(gpBase->mpPlayer2) gpBase->mpPlayer2->StopTerrorSound();

		//cLuxPlayerSanity::UpdateCheckEnemySeen skips any enemy that fails this.
		mbSanityDecreaseBeforePossess = mbCausesSanityDecrease;
		mbCausesSanityDecrease = false;

		//Known starting point. The possess helper re-picks walk/run every frame.
		SetMoveSpeed(eLuxEnemyMoveSpeed_Walk);

		//Park in Idle THROUGH the normal transition, so whatever state it was in runs
		//its kLuxOnLeave -- that is what puts mfFOVMul back to 1, undoes the per-state
		//speed multipliers, and de-registers music the way the state itself expects.
		//mbPossessed is already true, so the whitelist in ChangeState lets Idle past.
		ChangeState(eLuxEnemyState_Idle);
	}
	else
	{
		mbCausesSanityDecrease = mbSanityDecreaseBeforePossess;
		mpMover->ResetStuckCounter();

		//Left in Idle on purpose. Restoring the state it held when possessed would
		//re-run that state's kLuxOnEnter without the history that earned it -- Hunt
		//would re-add terror and attack music for a player it has not actually seen.
		//From Idle the senses come back next frame and it re-acquires on its own,
		//which is what "the AI comes back as normal" should mean.
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::SendMessage(eLuxEnemyMessage aType, float afTime, bool abLocalScope, const cVector3f& avX,float afX, int alX)
{
	if(mbDisabled) return;
	if(	TriggersDisabled() && 
		aType > eLuxEnemyMessage_EndOfPath && 
		aType != eLuxEnemyMessage_PlayerInRange && 
		aType != eLuxEnemyMessage_PlayerOutOfRange
		) 
	{
		return;
	}

	if(aType == eLuxEnemyMessage_PlayerInRange)
	{
		gpBase->mpProgressLogHandler->AddLog(eLuxProgressLogLevel_Low, "Enemy '"+msName+"'  is in range");
	}

	cLuxStateMessage message;
	message.mfCount = afTime;
	message.mfTime = afTime;
	message.mState = abLocalScope ? mCurrentState : eLuxEnemyState_LastEnum;
	message.mType = aType;
	message.mvCustomValue = avX;
	message.mfCustomValue = afX;
	message.mlCustomValue = alX;

	mlstMessages.push_back(message);
}

//-----------------------------------------------------------------------

void iLuxEnemy::PlayAnim(	const tString &asName, bool abLoop, float afFadeTime,
							bool abDependsOnSpeed, float afSpeedMul,
							bool abSyncWithPrevFrame,
							bool abOverideMoveState,
							bool abUseMoveAnimWhenCurrentIsOver)
{
	//if not using animations, then return.
	if(mbUseAnimations==false) return;

	//Check if the animation is already playing.
	if(	mpCurrentAnimation != NULL && 
		mpCurrentAnimation->GetName() == asName && 
		mpCurrentAnimation->IsActive() &&
		mpCurrentAnimation->IsOver()== false)
	{
		return;
	}

	//////////////////////////
	//Get animation
	cAnimationState *pNewAnim = mpMeshEntity->GetAnimationStateFromName(asName);
	if(pNewAnim==NULL)
	{
		Warning("Animation '%s' does not exist!\n",asName.c_str());
		return;
	}

	//////////////////////////
	//Start animation and fade previous
	pNewAnim->SetActive(true);
	if(mpCurrentAnimation && mpCurrentAnimation != pNewAnim) 
	{
		mpCurrentAnimation->FadeOut(afFadeTime);

		if(pNewAnim->IsFading()==false) pNewAnim->SetWeight(0);
		pNewAnim->FadeIn(afFadeTime);
	}
	else
	{
		pNewAnim->SetWeight(1.0f);
	}
	pNewAnim->SetLoop(abLoop);

	/////////////////////////////////////////
	//Check if this animation should start at the same place as the previous
	if(abSyncWithPrevFrame && mpCurrentAnimation)
	{
		pNewAnim->SetRelativeTimePosition(mpCurrentAnimation->GetRelativeTimePosition());
	}
	else
	{
		pNewAnim->SetTimePosition(0);
	}


	mpCurrentAnimation  = pNewAnim;

	mbAnimationIsSpeedDependant = abDependsOnSpeed;
	mfAnimationSpeedMul = afSpeedMul;
	mbUseMoveAnimWhenCurrentIsOver = abUseMoveAnimWhenCurrentIsOver;

	mpMover->SetOverideMoveState(abOverideMoveState);
}

//-----------------------------------------------------------------------

void iLuxEnemy::FadeOutCurrentAnim(float afFadeTime)
{
	if(mpCurrentAnimation==NULL) return;
	if(mpCurrentAnimation->IsLooping()) return;

	mpCurrentAnimation->FadeOut(afFadeTime);

	mpCurrentAnimation = NULL;

	if(mbUseMoveAnimWhenCurrentIsOver) mpMover->UseMoveStateAnimations();
}

//-----------------------------------------------------------------------

float iLuxEnemy::ConvertAnimToAbsoluteTime(float afRelativeTimePostion)
{
	if(mpCurrentAnimation==NULL) return 0;

    return mpCurrentAnimation->GetLength() * afRelativeTimePostion;	
}

//-----------------------------------------------------------------------

cSoundEntity* iLuxEnemy::PlaySound(const tString &asName)
{
	if(asName=="") return NULL;

	cSoundEntity *pSound = mpMap->GetWorld()->CreateSoundEntity("EnemySound", asName, true);
	if(pSound)
	{
		//pSound->SetPosition(mpCharBody->GetPosition());
		if(mpMeshEntity) mpMeshEntity->AddChild(pSound);
	}

	return pSound;
}

//-----------------------------------------------------------------------

void iLuxEnemy::SetPositionAtStartPos()
{
	mpCharBody->SetFeetPosition(mvStartPosition);
}

//-----------------------------------------------------------------------

void iLuxEnemy::ResetProperties()
{
	SetPositionAtStartPos();
	ChangeState(eLuxEnemyState_Idle);

	mbCausesSanityDecrease = mbCausesSanityDecreaseAsDefault;

	mbDisabled = false;
	mfLookForPlayerCount =0;
	mbCanSeePlayer = false;
	mlPlayerInLOSCount =0;
	mbPlayerDetected = false;
	mbPlayerInRange = false;

	mfCheckAtDoorCount =0;
	mbStuckAtDoor = false;

	mfDarknessGlowAlpha =0;
	mfDarknessGlowUpdateCount =0;

	mfFOVMul = 1.0f;

	mfDamageCount =0;

	mvLastKnownPlayerPos =0;
	mfLastPlayerPosCount =0;

	mpTargetPlayer = NULL;
	mfRetargetCount =0;

	mlCurrentPatrolNode = 0;

	mlAttackHitCounter =0;

	mbIsSeenByPlayer = false;

	mbSkipVisibilityRangeHandicaps = false;

	SetMoveSpeed(eLuxEnemyMoveSpeed_Walk);
		
	OnResetProperties();
}

//-----------------------------------------------------------------------

void iLuxEnemy::FadeToSmoke(bool abPlaySound)
{
	///////////////////////////
	//Add particles at skeleton bones
	if(mpMeshEntity->GetMesh()->GetSkeleton())
	{
		cWorld *pWorld = mpMap->GetWorld();
		for(int i=0; i<mpMeshEntity->GetBoneStateNum(); ++i)
		{
			cBoneState *pBone = mpMeshEntity->GetBoneState(i);

			//Check "size" of bone
			tString sPSFile = "ps_enemy_disappear_per_bone.ps";
			if(pBone->GetParent())
			{
				float fDist = cMath::Vector3Dist(pBone->GetLocalPosition(), pBone->GetParent()->GetLocalPosition());
				if(fDist < 0.1f) sPSFile = "ps_enemy_disappear_per_bone_small.ps";
			}

			cParticleSystem *pPS = pWorld->CreateParticleSystem("Disappear",sPSFile,0.3f);
			pPS->SetPosition(pBone->GetWorldPosition());
		}
	}

	///////////////////////////
	//Dissolve mesh
	if(mpMeshEntity->IsVisible())
		gpBase->mpMapHandler->GetCurrentMap()->AddDissolveEntity(mpMeshEntity, 1);
	SetActive(false);

	///////////////////////////
	//Play sound
	if(abPlaySound)
		gpBase->mpHelpFuncs->PlayGuiSoundData("enemy_hallucination_disappear", eSoundEntryType_Gui);
}

//-----------------------------------------------------------------------

void iLuxEnemy::AddPatrolNode(cAINode *apNode, float afWaitTime, const tString & asAnimation, bool abLoopAnimation)
{
	cLuxEnemyPatrolNode patrolNode;
	
	patrolNode.mfWaitTime = afWaitTime;
	patrolNode.mpNode = apNode;
	patrolNode.msAnimation = asAnimation;
	patrolNode.mbLoopAnimation = abLoopAnimation;

    mvPatrolNodes.push_back(patrolNode);
}

void iLuxEnemy::ClearPatrolNodes()
{
	mvPatrolNodes.clear();

	mlCurrentPatrolNode =0;
}

//-----------------------------------------------------------------------

cLuxEnemyPatrolNode* iLuxEnemy::GetCurrentPatrolNode()
{
	if(mlCurrentPatrolNode>= (int)mvPatrolNodes.size() || mlCurrentPatrolNode<0)
		return NULL;

	return &mvPatrolNodes[mlCurrentPatrolNode];
}

bool iLuxEnemy::IsAtLastPatrolNode()
{
	return mlCurrentPatrolNode >= (int)mvPatrolNodes.size()-1;
}

void iLuxEnemy::IncCurrentPatrolNode(bool abLoopIfAtEnd)
{
	mlCurrentPatrolNode++;
	if(mlCurrentPatrolNode >= (int)mvPatrolNodes.size())
	{
		if(abLoopIfAtEnd)
			mlCurrentPatrolNode =0;
		else
			mlCurrentPatrolNode = (int)mvPatrolNodes.size()-1;
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::SetDisableTriggers(bool abX)
{
	mbDisableTriggers = abX;

	if(mbDisableTriggers)
	{
		OnDisableTriggers();
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::SetDisabled(bool abX)
{
	mbDisabled = abX;

	if(mbDisabled)
	{
		mpPathfinder->Stop();
	}
}

//-----------------------------------------------------------------------

bool iLuxEnemy::InRangeOfFood(iPhysicsBody *apFoodBody)
{
	//TODO: Some general check here?
	return false;
}

//-----------------------------------------------------------------------

string& iLuxEnemy::GetCurrentEnemyStateName()
{
	msPolledEnemyStateName = cString::To8Char(gsLuxEnemyStates[mCurrentState]);
	return msPolledEnemyStateName;
}

//-----------------------------------------------------------------------

float iLuxEnemy::DrawDebug(cGuiSet *apSet,iFontData *apFont,float afStartY)
{
	if(mbDisabled) return afStartY;

    apSet->DrawFont(apFont, cVector3f(5,afStartY,10),13,cColor(1,1), _W("Name: '%ls'"),cString::To16Char(msName).c_str());
	afStartY += 14;

	apSet->DrawFont(apFont, cVector3f(5,afStartY,10),13,cColor(1,1), 
		_W("  State: '%ls' PlayerSeen: %d PlayerDetected: %d PlayerInRange: %d StuckAtDoor: %d FOVMul: %f"),
		gsLuxEnemyStates[mCurrentState].c_str(), mbCanSeePlayer ? 1 : 0, mbPlayerDetected ? 1 : 0, mbPlayerInRange ? 1 : 0, mbStuckAtDoor ? 1 : 0,
		mfFOVMul);
	afStartY += 14;


	apSet->DrawFont(apFont, cVector3f(5,afStartY,10),13,cColor(1,1), 
		_W("  Health: %f FOVMul: %f DistFromStart: %f StuckCounter: %f Speed: %f SpeedPercent: %f Vel: %f"),
		mfHealth, mfFOVMul, cMath::Vector3Dist(mpCharBody->GetFeetPosition(), mvStartPosition), mpMover->GetStuckCounter(),mpMover->GetMoveSpeed(), mpMover->GetWantedSpeedAmount());
	afStartY += 14;

	apSet->DrawFont(apFont, cVector3f(5,afStartY,10),13,cColor(1,1), 
		_W("  DistToPlayer: %f PlayerDirAmount: %f CurrentPatrolNode: %d MoverSpeedMul: %f"), DistToPlayer(), GetPlayerMovementTowardEnemyAmount(), 
						mlCurrentPatrolNode, mpMover->CalculateSpeedMul(1.0f/60.0f));
	afStartY += 14;

	//apSet->DrawFont(apFont, cVector3f(5,afStartY,10),13,cColor(1,1), 
	//	_W("  Climbing: %d"), mpCharBody->IsClimbing());
	//afStartY += 14;


	return afStartY;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

tString iLuxEnemy::GetCurrentPoseSuffix()
{
	switch(mCurrentPose)
	{
	case eLuxEnemyPoseType_Biped: return "Biped";
	case eLuxEnemyPoseType_Quadruped: return "Quadruped";

	}

	return "";
}

//-----------------------------------------------------------------------

bool iLuxEnemy::StateEvent(int alState, eLuxEnemyStateEvent aEvent, cLuxStateMessage *apMessage)
{
	bool bRet = StateEventImplement(alState, aEvent, apMessage);
	if(bRet==false)
		bRet = StateEventImplement(-1, aEvent, apMessage);
	
	return bRet;
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateStateMachine(float afTimeStep)
{
	//////////////////
	// Update
	CheckStateChange();

	// POSSESSION: skip the state's per-frame handler -- that IS the AI. What stays
	// is CheckStateChange and the message pump below, and both are load bearing:
	// an attack's damage arrives as eLuxEnemyMessage_AnimationSpecialEvent and its
	// exit as _AnimationOver, so freezing the whole machine would leave a swing
	// that never lands and never ends.
	if(mbPossessed==false)
		StateEvent(mCurrentState, eLuxEnemyStateEvent_Update, NULL);

	CheckStateChange();

	//////////////////
	// Check messages
	for(tLuxStateMessageListIt it = mlstMessages.begin(); it != mlstMessages.end(); )
	{
		cLuxStateMessage& message = *it;
		
		////////////////////////////
		//Check if the scope is right else remove
		if(message.mState != eLuxEnemyState_LastEnum && message.mState != mCurrentState)
		{
			it = mlstMessages.erase(it);
			continue;
		}
		
		////////////////////////////
		//Check if message is to be sent
		message.mfCount -= afTimeStep;
		if(message.mfCount <= 0)
		{
			StateEvent(mCurrentState, eLuxEnemyStateEvent_Message, &message);
			it = mlstMessages.erase(it);
			continue;
		}

		////////////////////////////
		//Inc iterator
		++it;
	}

	CheckStateChange();
}

//-----------------------------------------------------------------------

void iLuxEnemy::CheckStateChange()
{
	if(mNextState != mCurrentState)
	{
		if(mCurrentState != eLuxEnemyState_LastEnum)
			StateEvent(mCurrentState, eLuxEnemyStateEvent_Leave, NULL);
		
		mPreviousState = mCurrentState;
		mCurrentState = mNextState;
		StateEvent(mCurrentState, eLuxEnemyStateEvent_Enter, NULL);
	}

}

//-----------------------------------------------------------------------

void iLuxEnemy::ChangeSoundState(eLuxEnemySoundState aState)
{
	if(mSoundState == aState) return;

	mSoundState = aState;

    if(mpCurrentSound && mpMap->GetWorld()->SoundEntityExists(mpCurrentSound, mlCurrentSoundID))
	{
		mpCurrentSound->FadeOut(3.0f);
        mpCurrentSound = NULL;
	}

	mfAmbientSoundCount = cMath::RandRectf(mfAmbientSoundMinTime[mSoundState], mfAmbientSoundMaxTime[mSoundState]);
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateSoundState(float afTimeStep)
{
	if(mSoundState == eLuxEnemySoundState_Silent) return;
	if(msAmbientSound[mSoundState] == "") return;

	//If sound is playing still do nothing.
	if(mpCurrentSound && mpMap->GetWorld()->SoundEntityExists(mpCurrentSound,mlCurrentSoundID)) return;
	
	mpCurrentSound = NULL;
	
	
	if(mfAmbientSoundCount <=0)
	{
		mpCurrentSound = PlaySound(msAmbientSound[mSoundState]);
		if(mpCurrentSound )mlCurrentSoundID = mpCurrentSound->GetCreationID();
		mfAmbientSoundCount = cMath::RandRectf(mfAmbientSoundMinTime[mSoundState], mfAmbientSoundMaxTime[mSoundState]);
	}
	else
	{
		mfAmbientSoundCount -= afTimeStep;
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateAnimation(float afTimeStep)
{
	if(mbUseAnimations==false) return;
	if(mpMover->GetOverideMoveState()==false || mpCurrentAnimation==NULL) return;


	//////////////////
	// Check for special event
	if(	mpCurrentAnimation->GetPreviousTimePosition() <= mpCurrentAnimation->GetSpecialEventTime() &&
		mpCurrentAnimation->GetTimePosition() > mpCurrentAnimation->GetSpecialEventTime())
	{
		SendMessage(eLuxEnemyMessage_AnimationSpecialEvent,0,false);
	}

	//////////////////
	// Check if over
	if(mpCurrentAnimation->IsOver())
	{
		if(mbUseMoveAnimWhenCurrentIsOver) mpMover->UseMoveStateAnimations();
		SendMessage(eLuxEnemyMessage_AnimationOver,0,false);
	}

	//////////////////
	// Update speed if needed.
	if(mbAnimationIsSpeedDependant)
	{
		float fSpeed = mpCharBody->GetVelocity(afTimeStep).Length();
		if(mpCharBody->GetMoveSpeed(eCharDir_Forward) <0) fSpeed = -fSpeed;

		mpCurrentAnimation->SetSpeed(std::fabs(fSpeed) * mfMoveSpeedAnimMul);
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateCharBody(float afTimeStep)
{
	float fMul = mpMover->CalculateSpeedMul(afTimeStep);

	mpCharBody->SetMaxPositiveMoveSpeed(eCharDir_Forward, mfForwardSpeed*fMul);
	mpCharBody->SetMaxNegativeMoveSpeed(eCharDir_Forward, mfBackwardSpeed*fMul);
	mpCharBody->SetMoveAcc(eCharDir_Forward,mfForwardAcc);
	mpCharBody->SetMoveDeacc(eCharDir_Forward,mfForwardDeacc);
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateCanSeePlayer(float afTimeStep)
{
	cLuxPlayer *pPlayer = GetTargetPlayer();

	if(pPlayer->IsDead()) return;

	if(TriggersDisabled())
	{
		mbCanSeePlayer = false;
	}


	////////////////////////////////
	//Check if it is time to check for player
	mfLookForPlayerCount-= afTimeStep;
	if(mfLookForPlayerCount > 0) return;
	mfLookForPlayerCount = 0.3f;

	/////////////////////////////
	// Set up variables
	iCharacterBody *pPlayerBody = pPlayer->GetCharacterBody();

	float fDist = cMath::Vector3Dist(pPlayerBody->GetPosition(), mpCharBody->GetPosition());

	////////////////////////
	// Check range (lantern / darkness / crouch handicaps included)
	float fMaxRange = CalcPlayerSeeRange(pPlayer);

	if(fMaxRange < fDist)
	{
		if(mbCanSeePlayer==false) SendMessage(eLuxEnemyMessage_PlayerUnseen,0,false);
		mbCanSeePlayer = false;
		mlPlayerInLOSCount=0;
		return;
	}
	
	////////////////////////
	// Get the min dist (use to check if player is close enough to be seen no matter what)
	float fMinDist =	mpCharBody->GetCurrentBody()->GetBoundingVolume()->GetRadius() + 
						pPlayerBody->GetCurrentBody()->GetBoundingVolume()->GetRadius() + 0.05f;
	
	//Skip the FOV test if really close to the player.
	bool bUseFOV = (fDist >= fMinDist);

	////////////////////////////////
	//Player is in LOS
	//TODO: Skipping closer than min dist test! Put back?
	if(LineOfSight(pPlayerBody->GetPosition(), pPlayerBody->GetSize(), bUseFOV))
	{
		if(mlPlayerInLOSCount >= 2)
		{
			if(mbCanSeePlayer==false)
			{
				SendMessage(eLuxEnemyMessage_PlayerSeen,0,false);
			}
			mbCanSeePlayer = true;
		}
		else
		{
			mlPlayerInLOSCount++;
		}
	}
	////////////////////////////////
	//Player is NOT in LOS
	else
	{
		if(mlPlayerInLOSCount>0)
		{
			mlPlayerInLOSCount=0;

			if(mbCanSeePlayer==false)
			{
				SendMessage(eLuxEnemyMessage_PlayerUnseen,0,false);
			}
			mbCanSeePlayer = false;
		}
	}

}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdatePlayerDetected(float afTimeStep)
{
	if(GetTargetPlayer()->IsDead()) return;

	bool bDetected = PlayerIsDetected();

	if(bDetected && mbPlayerDetected==false)
	{
		SendMessage(eLuxEnemyMessage_PlayerDetected, 0, false);
	}
	else if(bDetected==false && mbPlayerDetected)
	{
		SendMessage(eLuxEnemyMessage_PlayerUndetected, 0, false);
	}

	mbPlayerDetected = bDetected;
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdatePlayerInRange(float afTimeStep)
{
	if(GetTargetPlayer()->IsDead()) return;

	float fDistance = DistToPlayer();

	if(fDistance <= mfActivationDistance && mbPlayerInRange==false)
	{
		mbPlayerInRange = true;
		SendMessage(eLuxEnemyMessage_PlayerInRange);
	}
	else if(fDistance > mfActivationDistance*1.25f+2.0f && mbPlayerInRange)
	{
		mbPlayerInRange = false;
		SendMessage(eLuxEnemyMessage_PlayerOutOfRange);
	}

}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateCheckStuckAtDoor(float afTimeStep)
{
	if(mpMover->GetStuckCounter() < 0.1f)
	{
		mbStuckAtDoor = false;
		return;
	}

	mfCheckAtDoorCount -= afTimeStep;
	if(mfCheckAtDoorCount > 0.0f) return;
	mfCheckAtDoorCount = 0.1f;

	bool bAtDoor = false;
	mlStuckDoorID = -1;

	cBoundingVolume *pBV = mpCharBody->GetCurrentBody()->GetBoundingVolume();
	iLuxProp *pSwingDoor = NULL;

	
	////////////////////////////////
	// Get the diretion of the player
	// If moving with path finder get the dir by getting next goal position.
	cVector3f vPlayerDir;
	if(mpPathfinder->IsMoving())
		vPlayerDir = cMath::Vector3Normalize(mpPathfinder->GetNextGoalPos() - mpCharBody->GetPosition());
	else
		vPlayerDir = mpCharBody->GetForward();


	//////////////////////
	// Iterate entities and look for a door overlapping
    cLuxEntityIterator entIt = mpMap->GetEntityIterator();
	while(entIt.HasNext())
	{
		iLuxEntity *pEntity = entIt.Next();
		if(pEntity->GetEntityType() != eLuxEntityType_Prop) continue;
		if(pEntity->GetDestroyMe()) continue;

		iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
		if(pProp->GetPropType() != eLuxPropType_SwingDoor) continue;
		
		for(int i=0; i<pProp->GetBodyNum(); ++i)
		{
			iPhysicsBody *pBody = pProp->GetBody(i);
			if(pBody->GetMass()==0) continue;
			
			//////////////////////
			//Check BV intersection
			if(cMath::CheckBVIntersection(*pBV, *pBody->GetBoundingVolume())==false)
			{
				continue;
			}

            //Calculate direction to door.
			cVector3f vToDoor = cMath::Vector3Normalize(pBody->GetLocalPosition() - mpCharBody->GetPosition());
			float fDot = cMath::Vector3Dot(vPlayerDir, vToDoor);
			
			/////////////////////////////
			//Check if player faces door
			if(fDot > 0.2f)
			{
				mlStuckDoorID = pProp->GetID();
				pSwingDoor = pProp;
				bAtDoor = true;
				break;
			}
		}
		
		if(bAtDoor) break;
	}

	//////////////////////
	// Change state if needed and send message
	if(bAtDoor && mbStuckAtDoor==false)
	{
		mbStuckAtDoor = true;
		mpMover->ResetStuckCounter();
		SendMessage(eLuxEnemyMessage_StuckAtDoor,0,false, pSwingDoor->GetMainBody()->GetWorldPosition());
	}
	else if(bAtDoor==false && mbStuckAtDoor)
	{
		mbStuckAtDoor = false;
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateCheckLastPlayerPos(float afTimeStep)
{
	if(CanSeePlayer())
	{
		mfLastPlayerPosCount += afTimeStep;
		if(mfLastPlayerPosCount > 1.0f) mfLastPlayerPosCount = 1.0f;
	}
	else
	{
		mfLastPlayerPosCount -= afTimeStep;
		if(mfLastPlayerPosCount < 0) mfLastPlayerPosCount=0;
	}

	if(mfLastPlayerPosCount > 0)
	{
		mvLastKnownPlayerPos = GetTargetPlayer()->GetCharacterBody()->GetFeetPosition();
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateDarknessGlow(float afTimeStep)
{
	///////////////////////////////
	//Update darkness alpha according to goal
	if(mfDarknessGlowAlpha != mfDarknessGlowAlphaGoal)
	{
		if(mfDarknessGlowAlpha > mfDarknessGlowAlphaGoal)
		{
			mfDarknessGlowAlpha -= afTimeStep;
			if(mfDarknessGlowAlpha < mfDarknessGlowAlphaGoal) mfDarknessGlowAlpha = mfDarknessGlowAlphaGoal;
		}
		else
		{
			mfDarknessGlowAlpha += afTimeStep;
			if(mfDarknessGlowAlpha > mfDarknessGlowAlphaGoal) mfDarknessGlowAlpha = mfDarknessGlowAlphaGoal;
		}
	}

    if(mfDarknessGlowAlpha > 0)
	{
		//////////////////////////////
		// Coop: the glow is a viewer effect, so check BOTH players' cameras
		// and use the closest one that has the enemy in its frustum.
		cLuxPlayer *pViewers[2] = { gpBase->mpPlayer, gpBase->mpPlayer2 };
		cLuxPlayer *pBestViewer = NULL;
		float fBestDistSqrt = 0;

		for(int i=0; i<2; ++i)
		{
			cLuxPlayer *pViewer = pViewers[i];
			if(pViewer==NULL || pViewer->GetCamera()==NULL) continue;

			cCamera *pCam = pViewer->GetCamera();
			if(pCam->GetFrustum()->CollideBoundingVolume(mpMeshEntity->GetBoundingVolume()) == eCollision_Outside)
				continue;

			float fDistSqrt = cMath::Vector3DistSqr(pCam->GetPosition(), mpCharBody->GetPosition());
			if(pBestViewer==NULL || fDistSqrt < fBestDistSqrt)
			{
				pBestViewer = pViewer;
				fBestDistSqrt = fDistSqrt;
			}
		}

		if(pBestViewer && fBestDistSqrt < mfEnemyDarknessGlowMaxDistance*mfEnemyDarknessGlowMaxDistance)
		{
			float fDist = sqrtf(fBestDistSqrt);

			float fDistMul=1.0f;
			if(fDist > mfEnemyDarknessGlowMaxDistance * 0.5f)
			{
				fDistMul = 1 - ((fDist-mfEnemyDarknessGlowMaxDistance * 0.5f) / (mfEnemyDarknessGlowMaxDistance * 0.5f));
				if(fDistMul<0) fDistMul =0;
			}

			float fSanityMul =  pBestViewer->GetSanity()/100.0f;
			fSanityMul = sqrtf(fSanityMul);

			float fAlpha = mfDarknessGlowAlpha*mfDarknessGlowAlpha*fDistMul*fSanityMul;

			for(int i=0; i<mpMeshEntity->GetSubMeshEntityNum(); ++i)
			{
				gpBase->mpEffectRenderer->AddEnemyGlow(mpMeshEntity->GetSubMeshEntity(i),fAlpha);
			}
		}
	}


	////////////////////////////////
	//Check if darkness level should be updated
	if(mpMeshEntity->IsVisible()==false) return;
	if(mfDarknessGlowUpdateCount > 0)
	{
		mfDarknessGlowUpdateCount -=afTimeStep;
	}
	mfDarknessGlowUpdateCount = 0.3f;


	////////////////////////////////
	//Update darkness alpha goal
	mfDarknessGlowAlphaGoal = gpBase->mpMapHelper->GetLightLevelAtPos(mpCharBody->GetPosition());
	mfDarknessGlowAlphaGoal += gpBase->mpMapHelper->GetLightLevelAtPos(mpCharBody->GetFeetPosition()+cVector3f(0,0.1f,0));
	mfDarknessGlowAlphaGoal /= 2.0f;
	
	mfDarknessGlowAlphaGoal *= 2;
	if(mfDarknessGlowAlphaGoal>1.0f) mfDarknessGlowAlphaGoal = 1.0f;

	mfDarknessGlowAlphaGoal = 1.0f - mfDarknessGlowAlphaGoal;
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateRegenHealth(float afTimeStep)
{
	if(mfHealth <= 0) return;

    if(mfHealth < mfMaxRegenHealth && mfRegenHealthSpeed >0)
	{
		mfHealth += afTimeStep * mfRegenHealthSpeed;
		if(mfHealth > mfMaxRegenHealth) mfHealth = mfMaxRegenHealth;
	}
}

void iLuxEnemy::UpdateHallucination(float afTimeStep)
{
	if(mbHallucination==false || mfHealth <= 0) return;

	// Coop: EITHER player getting close dispels the hallucination.
	float fDistSqr = cMath::Vector3DistSqr(mpCharBody->GetPosition(), gpBase->mpPlayer->GetCharacterBody()->GetPosition());
	if(gpBase->mpPlayer2 && gpBase->mpPlayer2->GetCharacterBody())
	{
		float fDistSqrP2 = cMath::Vector3DistSqr(mpCharBody->GetPosition(), gpBase->mpPlayer2->GetCharacterBody()->GetPosition());
		if(fDistSqrP2 < fDistSqr) fDistSqr = fDistSqrP2;
	}
	if(fDistSqr < mfHallucinationEndDist * mfHallucinationEndDist)
	{
		FadeToSmoke(true);
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateAlignEntityWithGroundRay(float afTimeStep)
{
	if(mbAlignEntityWithGroundRay==false) return;

	mfCheckGroundRayCount -= afTimeStep;
	if(mfCheckGroundRayCount > 0) return;

	mfCheckGroundRayCount = 0.02f;

	///////////////////////////////////
	// Get the distance to ground
	float fStartAdd = 0.05f;
	float fDist=0;
	cVector3f vNormal;

	//Calculate the start postion depending the way that the character faces.
	cVector3f vMoveDir = cMath::MatrixMul(cMath::MatrixRotateY(mpCharBody->GetYaw()),cVector3f(0,0,-1));
	cVector3f vStartPos = mpCharBody->GetFeetPosition()+cVector3f(0,fStartAdd,0) + vMoveDir * mpCharBody->GetSize().x*0.5f;

    bool bIntersect = gpBase->mpMapHelper->GetClosestCharCollider(vStartPos, cVector3f(0,-1,0),0.5f,&fDist,&vNormal,NULL);
    if(bIntersect==false) return;
	fDist -= fStartAdd;

	///////////////////////////////////
	// Add dist to list, pop oldest element if full
	mlstGroundCheckDists.push_back(fDist);
	if((int)mlstGroundCheckDists.size() > mlMaxGroundCheckDists)
		mlstGroundCheckDists.pop_front();

	///////////////////////////////////
	// Calculate avg dist.
	float fFinalDist =0;
	for(tFloatListIt it = mlstGroundCheckDists.begin(); it != mlstGroundCheckDists.end(); ++it)
	{
		fFinalDist += *it;
	}
	fFinalDist /= (float)mlstGroundCheckDists.size();
	
	///////////////////////////////////
	// Set the offset matrix
	cMatrixf mtxOffset = m_mtxCharMeshOffset;
	mtxOffset.SetTranslation(mtxOffset.GetTranslation() + cVector3f(0,-fFinalDist,0));
    mpCharBody->SetEntityOffset(mtxOffset);
}

//-----------------------------------------------------------------------

cLuxPlayer* iLuxEnemy::GetTargetPlayer()
{
	cLuxPlayer *pP2 = gpBase->mpPlayer2;

	////////////////////////////////
	// No coop (or no P2): always Player 1. Also catches a stale pointer left
	// behind after P2 was destroyed — compared, never dereferenced.
	if(gpBase->mpMapHandler->GetCoopMode()==false || pP2==NULL || pP2->GetCharacterBody()==NULL)
	{
		mpTargetPlayer = gpBase->mpPlayer;
	}
	else if(mpTargetPlayer != gpBase->mpPlayer && mpTargetPlayer != pP2)
	{
		mpTargetPlayer = gpBase->mpPlayer;
	}

	return mpTargetPlayer;
}

//-----------------------------------------------------------------------

float iLuxEnemy::CalcPlayerSeeRange(cLuxPlayer *apPlayer)
{
	bool bLanternOn = apPlayer->GetHelperLantern()->IsActive();

	////////////////////////
	// Check if crouching
	bool bCrouching = false;
	if(apPlayer->GetCurrentMoveState() == eLuxMoveState_Normal)
	{
		cLuxMoveState_Normal *pNormalMove = static_cast<cLuxMoveState_Normal*>(apPlayer->GetCurrentMoveStateData());
		if(pNormalMove->IsCrouching()) bCrouching = true;
	}

	////////////////////////
	// Base range. NOTE: vanilla had 'mfSightRange *= 1.5f' here, permanently
	// compounding the member every sense tick while the lantern was lit.
	float fMaxRange = mfSightRange;

	if(bLanternOn) fMaxRange = mfSightRange * 1.5f;

	if(bLanternOn==false && apPlayer->GetHelperLightLevel()->GetNormalLightLevel() < mfPlayerInDarknessLightLevel &&
		apPlayer->GetAvgSpeed() < 0.05f && mbSkipVisibilityRangeHandicaps==false)
		fMaxRange = mfDarknessSightRange;

	if(bCrouching && bLanternOn==false && mbSkipVisibilityRangeHandicaps==false)
		fMaxRange = fMaxRange * mfCrouchVisibleRangeMul;

	return fMaxRange;
}

//-----------------------------------------------------------------------

bool iLuxEnemy::CanDetectPlayerNow(cLuxPlayer *apPlayer)
{
	if(apPlayer==NULL || apPlayer->GetCharacterBody()==NULL || apPlayer->IsDead()) return false;

	iCharacterBody *pBody = apPlayer->GetCharacterBody();

	float fDist = cMath::Vector3Dist(pBody->GetPosition(), mpCharBody->GetPosition());
	if(fDist > CalcPlayerSeeRange(apPlayer)) return false;

	return LineOfSight(pBody->GetPosition(), pBody->GetSize(), true);
}

//-----------------------------------------------------------------------

void iLuxEnemy::UpdateTargetPlayer(float afTimeStep)
{
	cLuxPlayer *pP1 = gpBase->mpPlayer;
	cLuxPlayer *pP2 = gpBase->mpPlayer2;

	// Validates the pointer and resolves to P1 when coop is off / P2 is gone.
	cLuxPlayer *pCurrent = GetTargetPlayer();

	if(gpBase->mpMapHandler->GetCoopMode()==false || pP2==NULL || pP2->GetCharacterBody()==NULL) return;

	////////////////////////////////
	// Dead players are never targets (with shared death both go down
	// together, but be safe about the in-between frames).
	bool bP1Alive = pP1->IsDead()==false;
	bool bP2Alive = pP2->IsDead()==false;

	if(bP1Alive != bP2Alive)
	{
		mpTargetPlayer = bP1Alive ? pP1 : pP2;
		if(pCurrent != mpTargetPlayer) pCurrent->RemoveTerrorEnemy(this);
		return;
	}
	if(bP1Alive==false) { mpTargetPlayer = pP1; return; }

	////////////////////////////////
	// Only re-evaluate every so often — sensing runs in between, and
	// switching every tick would make the enemy dither between players.
	mfRetargetCount -= afTimeStep;
	if(mfRetargetCount > 0) return;
	mfRetargetCount = 0.75f;

	bool bSee1 = CanDetectPlayerNow(pP1);
	bool bSee2 = CanDetectPlayerNow(pP2);

	float fDist1 = DistToChar(pP1->GetCharacterBody());
	float fDist2 = DistToChar(pP2->GetCharacterBody());

	////////////////////////////////
	// Both players in view: go for whoever — random pick on a fresh
	// detection, sticky afterwards — UNLESS one is obviously closer (under
	// ~60% of the other's distance), then that one wins.
	if(bSee1 && bSee2)
	{
		if(fDist1 < fDist2 * 0.6f)		mpTargetPlayer = pP1;
		else if(fDist2 < fDist1 * 0.6f)	mpTargetPlayer = pP2;
		else if(mbPlayerDetected==false)
			mpTargetPlayer = cMath::RandRectl(0,1)==0 ? pP1 : pP2;
		//else: keep the current target.
	}
	////////////////////////////////
	// Exactly one in view: that one.
	else if(bSee1) mpTargetPlayer = pP1;
	else if(bSee2) mpTargetPlayer = pP2;
	////////////////////////////////
	// No one in view: while calm, focus on whoever is nearest (so range
	// activation and first detection work for either player). While alerted
	// or hunting, stay on the current target's trail.
	else
	{
		if(	mCurrentState == eLuxEnemyState_Idle || mCurrentState == eLuxEnemyState_Wait ||
			mCurrentState == eLuxEnemyState_Patrol || mCurrentState == eLuxEnemyState_GoHome)
		{
			mpTargetPlayer = fDist2 < fDist1 ? pP2 : pP1;
		}
	}

	////////////////////////////////
	// Terror follows the target: when switching away from a player, stop
	// contributing to their heartbeat.
	if(pCurrent != mpTargetPlayer)
	{
		pCurrent->RemoveTerrorEnemy(this);
		ShowPlayerPosition(); //Refresh the last known pos to the new target.
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::RemoveTerrorFromAllPlayers()
{
	gpBase->mpPlayer->RemoveTerrorEnemy(this);
	if(gpBase->mpPlayer2) gpBase->mpPlayer2->RemoveTerrorEnemy(this);
}

//-----------------------------------------------------------------------

bool iLuxEnemy::TriggersDisabled()
{
	if(mbDisableTriggers) return true;

	if(gpBase->mpPlayer->GetHelperFlashback()->IsActive()) return true;

	// Coop: P2's flashbacks pause enemies too.
	cLuxPlayer *pP2 = gpBase->mpPlayer2;
	if(pP2 && pP2->GetHelperFlashback()->IsActive()) return true;

	return false;
}

//-----------------------------------------------------------------------

bool iLuxEnemy::Attack(const cEnemyAttackSizeData &aSizeData, const cEnemyAttackDamageData &aDamageData, float afDamageMul)
{
	bool bHitPlayer = false;
	bool bHit = gpBase->mpMapHelper->ShapeDamage(GetAttackShape(aSizeData.mlShapeIdx), GetDamageShapeMatrix(aSizeData.mvOffset), mpCharBody->GetPosition(),
												aDamageData.mfMinDamage*afDamageMul, aDamageData.mfMaxDamage*afDamageMul, 
												aDamageData.mfForce, aDamageData.mfMaxImpulse, 
												aDamageData.mlStrength, aDamageData.mfHitSpeed,
												aDamageData.mDamageType, aDamageData.mWeaponHitType,
												false, aDamageData.mbCheckPlayer, aDamageData.mbCheckProps, true,
												&bHitPlayer);	
	if(bHit)
	{
		mlAttackHitCounter++;
		PlaySound(aDamageData.msHitSound);
	}

	return bHitPlayer;
}

//-----------------------------------------------------------------------

void iLuxEnemy::SetMoveSpeed(eLuxEnemyMoveSpeed aType)
{
	mfForwardSpeed = mfDefaultForwardSpeed[mCurrentPose][aType];
	mfBackwardSpeed = mfDefaultBackwardSpeed[mCurrentPose][aType];
	mfForwardAcc = mfDefaultForwardAcc[mCurrentPose][aType];
	mfForwardDeacc = mfDefaultForwardDeacc[mCurrentPose][aType];

}

//-----------------------------------------------------------------------

cAINode *iLuxEnemy::GetSearchForPlayerNode(int alMaxIterations, float afMaxAngleMul, float afMinDistMul, float afMaxDistMul)
{
	float fMaxAngle = mfPlayerSearchMaxAngle * afMaxAngleMul;

	////////////////////
	//Iterate until a node is found or max iterations is reached.
	for(int i=0; i<alMaxIterations; ++i)
	{
		//Get the angle 
		float fAngle = cMath::RandRectf(-fMaxAngle, fMaxAngle);

		//Get the distance and direction to player
		cVector3f vToPlayer = GetTargetPlayer()->GetCharacterBody()->GetFeetPosition() - mpCharBody->GetFeetPosition();
		float fToPlayerDist = vToPlayer.Length();
		vToPlayer.Normalize();

		//Rotate the direction to player 
		cVector3f vSearchDir = cMath::MatrixMul(cMath::MatrixRotateY(fAngle), vToPlayer);

		//The length to use
		float fLength = cMath::RandRectf(mfPlayerSearchMinDistMul*afMinDistMul, mfPlayerSearchMaxDistMul*afMaxDistMul) * fToPlayerDist;

		//The wanted positon
		cVector3f vWantedPos = mpCharBody->GetFeetPosition() + vSearchDir*fLength;

		//Find the node closests to the postion
		cAINode * pNode = mpPathfinder->GetNodeAtPos(vWantedPos, 0, fLength,true, false, true, mpLastSearchNode);
		if(pNode)
		{
			mpLastSearchNode = pNode;
			return pNode;
		}
	}
	
	////////////////////
	//Jut pick a random node around the enemy
	return mpPathfinder->GetNodeAtPos(mpCharBody->GetFeetPosition(), 0, 13.0f,false, false, true, mpLastSearchNode);
}

//-----------------------------------------------------------------------

cAINode *iLuxEnemy::GetPatrolAroundPlayerNode(int alMaxIterations, float afMaxAngleMul, float afMinDistMul, float afMaxDistMul)
{
	float fMaxAngle = mfPlayerPatrolMaxAngle * afMaxAngleMul;

	cVector3f vPlayerFeetPos = GetTargetPlayer()->GetCharacterBody()->GetFeetPosition();

	////////////////////
	//Iterate until a node is found or max iterations is reached.
	for(int i=0; i<alMaxIterations; ++i)
	{
		//Get the angle 
		float fAngle = cMath::RandRectf(-fMaxAngle, fMaxAngle);

		//Get the distance direction from player to enemy start position.
		cVector3f vToStart = mvStartPosition - vPlayerFeetPos;
		vToStart.Normalize();

		//Rotate the direction to player 
		cVector3f vSearchDir = cMath::MatrixMul(cMath::MatrixRotateY(fAngle), vToStart);

		//The length to use
		float fLength = cMath::RandRectf(mfPlayerPatrolMinDist*afMinDistMul, mfPlayerPatrolMaxDist*afMaxDistMul);

		//The wanted position
		cVector3f vWantedPos = vPlayerFeetPos + vSearchDir*fLength;

		//Find the node closets to the position
		cAINode * pNode = mpPathfinder->GetNodeAtPos(vWantedPos, 0, fLength,true, false, true, mpLastSearchNode);
		if(pNode)
		{
			mpLastSearchNode = pNode;
			return pNode;
		}
	}

	////////////////////
	//Jut pick a random node around the enemy
	return mpPathfinder->GetNodeAtPos(mpCharBody->GetFeetPosition(), 0, 13.0f,false, false, true, mpLastSearchNode);
}

//-----------------------------------------------------------------------

static const cVector2f gvPosAdds[] = {cVector2f(0,0),
										cVector2f(0.75,0),
										cVector2f(-0.75,0),
										cVector2f(0,1),
										cVector2f(0,-1),
										cVector2f(0.5,0.5),
										cVector2f(-0.5,0.5),
										cVector2f(0.25,0),
										cVector2f(-0.25,0)
};

bool iLuxEnemy::LineOfSight(const cVector3f &avPos, const cVector3f &avSize, bool abCheckFOV)
{
	return LineOfSight(avPos, avSize, abCheckFOV, mpCharBody->GetPosition() + cVector3f(0,mpCharBody->GetSize().y/2 - 0.1f, 0));
}

bool iLuxEnemy::LineOfSight(const cVector3f &avPos, const cVector3f &avSize, bool abCheckFOV, const cVector3f& avSourcePos)
{
	/////////////////////////////////////////
	//Get the start and end center for the test.
	cVector3f vStartCenter = avSourcePos;
	cVector3f vEndCenter = avPos;

	/////////////////////////////
	//Calculate the right vector
	const cVector3f vForward = cMath::Vector3Normalize(vEndCenter - vStartCenter);
	const cVector3f vUp = cVector3f(0,1.0f,0);
	const cVector3f vRight = cMath::Vector3Cross(vForward, vUp);

	////////////////////////////////////
	//Check if the pos is within FOV
	if(abCheckFOV)
	{
		if(InFOV(avPos)==false)
		{
			return false;
		}
	}

	/////////////////////////////////////
	//Get the half with and height. Make them a little smaller so that player can slide over junk on floor.
	const float fHalfWidth = avSize.x * 0.4f;
	const float fHalfHeight = avSize.y * 0.4f;

	////////////////////////////
	// Init variables
	//Count of 2 is need for a line of sight success.
	int lCount=0;
	const int lMaxAdds = 9;
	
	///////////////////////////////////
	//Iterate through all the rays.
	for(int i=0; i< lMaxAdds; ++i)
	{
		cVector3f vAdd = vRight * (gvPosAdds[i].x*fHalfWidth) + vUp * (gvPosAdds[i].y*fHalfHeight);
		cVector3f vStart = vStartCenter + vAdd;
		cVector3f vEnd = vEndCenter + vAdd;

		if(gpBase->mpMapHelper->CheckLineOfSight(vStart, vEnd,false))
		{
			lCount++;
		}

		if(lCount==2)
		{
			return true;
		}
	}
	
	return false;
}

//-----------------------------------------------------------------------

int iLuxEnemy::CreateAttackShape(cWorld *apWorld, cVector3f &avSize, eCollideShapeType aType)
{
	iPhysicsWorld *pPhysicsWorld = apWorld->GetPhysicsWorld();
	iCollideShape *pShape = NULL;
	
	switch(aType)
	{
	case eCollideShapeType_Box:
		pShape = pPhysicsWorld->CreateBoxShape(avSize, NULL);
		break;
	case eCollideShapeType_Sphere:
		pShape = pPhysicsWorld->CreateSphereShape(avSize, NULL);
		break;
	case eCollideShapeType_Cylinder:
		pShape = pPhysicsWorld->CreateCylinderShape(avSize.x, avSize.y, NULL);
		break;
	case eCollideShapeType_Capsule:
		pShape = pPhysicsWorld->CreateCapsuleShape(avSize.x, avSize.y, NULL);
		break;
	}

	if(pShape)
	{
		mvAttackShapes.push_back(pShape);
		return (int)mvAttackShapes.size() -1;
	}
	return -1;
}

//-----------------------------------------------------------------------

cMatrixf iLuxEnemy::GetDamageShapeMatrix(const cVector3f& avOffset)
{
	cVector3f vPos =	mpCharBody->GetPosition() +
						mpCharBody->GetRight() * avOffset.x +
						mpCharBody->GetUp() * avOffset.y +
						mpCharBody->GetForward() * avOffset.z;
		

	cVector3f vRot = cVector3f(0,mpCharBody->GetYaw(),0);
	cMatrixf mtxOffset = cMath::MatrixRotate(vRot,eEulerRotationOrder_XYZ);
	mtxOffset.SetTranslation(vPos);

	return mtxOffset;
}

//-----------------------------------------------------------------------

cLuxProp_Object* iLuxEnemy::GetClosestFood(float afMaxDist, float afMaxHeightDist)
{
	cLuxProp_Object *pOutputFood=NULL;
	float fShortestDist=-1;

	cLuxEntityIterator it = mpMap->GetEntityIterator();
	while(it.HasNext())
	{
		///////////////////////////////////////////
		// Check so type is correct
		iLuxEntity *pEntity = it.Next();
        if(pEntity->GetEntityType() != eLuxEntityType_Prop) continue;
		
		iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
		if(pProp->GetPropType() != eLuxPropType_Object) continue;

		cLuxProp_Object *pObject = static_cast<cLuxProp_Object*>(pProp);
		if(pObject->IsFood()==false) continue;

		///////////////////////////////////////////
		// Check if height is correct
       	float fFoodLowestY = pObject->GetBody(0)->GetBoundingVolume()->GetMin().y;

		if(cMath::Abs(fFoodLowestY - mpCharBody->GetFeetPosition().y) > afMaxHeightDist) continue;


		///////////////////////////////////////////
		// Check distance
		cVector3f vFoodPos = pObject->GetBody(0)->GetLocalPosition();

		float fDistSqr = cMath::Vector3DistSqr(mpCharBody->GetPosition(), vFoodPos);
		if(fDistSqr > afMaxDist*afMaxDist) continue;

		if(fShortestDist <0 || fDistSqr < fShortestDist)
		{
			fShortestDist = fDistSqr;
			pOutputFood = pObject;
		}
	}

	return pOutputFood;
}

//-----------------------------------------------------------------------

cVector3f iLuxEnemy::GetPlayerFeetPos()
{
	return GetTargetPlayer()->GetCharacterBody()->GetFeetPosition();
}

//-----------------------------------------------------------------------

float iLuxEnemy::Dist2D(const cVector3f &avPos)
{
	cVector3f vStart = mpCharBody->GetFeetPosition();
	cVector3f vEnd = avPos;
	vStart.y =0;
	vEnd.y =0;

	return cMath::Vector3Dist(vStart,vEnd);
}

float iLuxEnemy::DistToChar(iCharacterBody *apBody)
{
	return cMath::Vector3Dist(mpCharBody->GetFeetPosition(), apBody->GetFeetPosition());
}

float iLuxEnemy::DistToChar2D(iCharacterBody *apBody)
{
	cVector3f vStart = mpCharBody->GetFeetPosition();
	cVector3f vEnd = apBody->GetFeetPosition();
	vStart.y =0;
	vEnd.y =0;

	return cMath::Vector3Dist(vStart,vEnd);
}

float iLuxEnemy::AbsHeightDistToChar(iCharacterBody *apBody)
{
	cVector3f vStart = mpCharBody->GetFeetPosition();
	cVector3f vEnd = apBody->GetFeetPosition();

	return cMath::Abs(vStart.y - vEnd.y);
}

cVector3f iLuxEnemy::GetDirection2D(const cVector3f &avPos)
{
	cVector3f vDiff = avPos - mpCharBody->GetPosition();
	vDiff.y=0; vDiff.Normalize();
	return vDiff;
}

float iLuxEnemy::DistToPlayer()
{
	cLuxPlayer *pPlayer = GetTargetPlayer();
	if(pPlayer->IsDead()) return 100000.0f;

	return DistToChar(pPlayer->GetCharacterBody());
}

float iLuxEnemy::DistToPlayer2D()
{
	cLuxPlayer *pPlayer = GetTargetPlayer();
	if(pPlayer->IsDead()) return 100000.0f;

	return DistToChar2D(pPlayer->GetCharacterBody());
}

float iLuxEnemy::DistToPlayer2D(const cVector3f& avPos)
{
	return cMath::Vector2DistXZ(GetTargetPlayer()->GetCharacterBody()->GetPosition(), avPos);
}

float iLuxEnemy::AbsHeightDistToPlayer()
{
	cLuxPlayer *pPlayer = GetTargetPlayer();
	if(pPlayer->IsDead()) return 100000.0f;

	return AbsHeightDistToChar(pPlayer->GetCharacterBody());
}

cVector3f iLuxEnemy::GetDirection2DToPlayer()
{
	return GetDirection2D(GetTargetPlayer()->GetCharacterBody()->GetPosition());
}

//-----------------------------------------------------------------------

bool iLuxEnemy::IsSeenByPlayer()
{
	///////////////////////////////
	// Coop: seen means seen by ANY player.
	cLuxPlayer *pViewers[2] = { gpBase->mpPlayer, gpBase->mpPlayer2 };

	for(int i=0; i<2; ++i)
	{
		cLuxPlayer *pViewer = pViewers[i];
		if(pViewer==NULL || pViewer->GetCamera()==NULL || pViewer->IsDead()) continue;

		// Check frustum
		cFrustum *pFrustum = pViewer->GetCamera()->GetFrustum();
		if(pFrustum->CollideBoundingVolume(mpCharBody->GetCurrentBody()->GetBoundingVolume())==eCollision_Outside)
		{
			continue;
		}

		// Line of sight with rays
		cVector3f vPlayerEyePos = pViewer->GetCamera()->GetPosition();

		if(LineOfSight(mpCharBody->GetPosition(), mpCharBody->GetSize(), false,vPlayerEyePos)) return true;
	}

	return false;
}

//-----------------------------------------------------------------------

bool iLuxEnemy::IsInPlayerFovAtFeetPos(const cVector3f& avFeetPos)
{
	cBoundingVolume bv;
	bv = *mpCharBody->GetCurrentBody()->GetBoundingVolume();
	bv.SetPosition(avFeetPos + cVector3f(0, mpCharBody->GetSize().y/2, 0) );

	// Coop: counts if ANY player would have the position in view.
	cLuxPlayer *pViewers[2] = { gpBase->mpPlayer, gpBase->mpPlayer2 };
	for(int i=0; i<2; ++i)
	{
		if(pViewers[i]==NULL || pViewers[i]->GetCamera()==NULL) continue;

		cFrustum *pFrustum = pViewers[i]->GetCamera()->GetFrustum();
		if(pFrustum->CollideBoundingVolume(&bv)!=eCollision_Outside) return true;
	}
	return false;
}

//-----------------------------------------------------------------------

bool iLuxEnemy::IsVisibleToPlayerAtFeetPos(const cVector3f& avFeetPos)
{
	cVector3f vCharPos = avFeetPos;
	vCharPos.y += mpCharBody->GetSize().y/2;

	// Coop: visible means visible to ANY player.
	cLuxPlayer *pViewers[2] = { gpBase->mpPlayer, gpBase->mpPlayer2 };
	for(int i=0; i<2; ++i)
	{
		if(pViewers[i]==NULL || pViewers[i]->GetCamera()==NULL) continue;

		cVector3f vPlayerEyePos = pViewers[i]->GetCamera()->GetPosition();
		if(LineOfSight(vCharPos, mpCharBody->GetSize(), false, vPlayerEyePos)) return true;
	}
	return false;
}

//-----------------------------------------------------------------------

float iLuxEnemy::GetPlayerMovementTowardEnemyAmount()
{
	iCharacterBody *pPlayerBody = GetTargetPlayer()->GetCharacterBody();
	
	cVector3f vPlayerDir = pPlayerBody->GetVelocity(gpBase->mpEngine->GetStepSize());
	vPlayerDir.y =0;
	
	//If not moving much, return lowest amount
    if(vPlayerDir.SqrLength() < 0.1f*0.1f) return -1; 
	vPlayerDir.Normalize();

	cVector3f vPlayerToEnemyDir = mpCharBody->GetPosition() - pPlayerBody->GetPosition();
	vPlayerToEnemyDir.y =0; vPlayerToEnemyDir.Normalize();

	return cMath::Vector3Dot(vPlayerDir, vPlayerToEnemyDir);
}

//-----------------------------------------------------------------------

bool iLuxEnemy::OutsideStartRadius()
{
	float fDist = cMath::Vector3Dist(mpCharBody->GetFeetPosition(), mvStartPosition);

	return fDist > mfActivationDistance * 1.2f + 2.0f;
}

//-----------------------------------------------------------------------

bool iLuxEnemy::InFOV(const cVector3f &avPos)
{
	float fFOV = mfFOV * mfFOVMul;
    if(fFOV < k2Pif)
	{
		cVector3f vStartCenter = mpCharBody->GetPosition() + cVector3f(0,mpCharBody->GetSize().y/2 - 0.2f, 0); //Use eye pos

		const cVector3f vDirToPos = cMath::Vector3Normalize(avPos - vStartCenter);
		cVector3f vEnemyForward = mpCharBody->GetForward();

		cVector3f vToPlayerAngle = cMath::GetAngleFromPoints3D(0,vDirToPos);
		cVector3f vEnemyAngle = cMath::GetAngleFromPoints3D(0,vEnemyForward);

		float fAngleX = cMath::Abs(cMath::GetAngleDistanceRad(vToPlayerAngle.x,vEnemyAngle.x));
		float fAngleY = cMath::Abs(cMath::GetAngleDistanceRad(vToPlayerAngle.y,vEnemyAngle.y));

		if(fAngleY > fFOV*0.5f) return false;
		if(fAngleX > fFOV*mfFOVXMul*0.5f) return false;
	}
	return true;
}

bool iLuxEnemy::PlayerInFOV()
{
	return InFOV(GetTargetPlayer()->GetCharacterBody()->GetPosition());
}

//-----------------------------------------------------------------------

void iLuxEnemy::OnSetActive(bool abX)
{	
	if(mpCharBody)
	{
		mpCharBody->SetActive(abX);
	}

	///////////////////
	//Mesh entity
	if(mpMeshEntity)
	{
		mpMeshEntity->SetActive(abX);
		mpMeshEntity->SetVisible(abX);
	}

	//Remove from attack and search music
	if(abX==false)
	{
		gpBase->mpMusicHandler->RemoveEnemy(eLuxEnemyMusic_Attack,this);
		gpBase->mpMusicHandler->RemoveEnemy(eLuxEnemyMusic_Search,this);
		RemoveTerrorFromAllPlayers();

		ChangeState(eLuxEnemyState_Idle);
	}
	
	OnSetActiveEnemySpecific(abX);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------
kBeginSerializeBase(cLuxStateMessage_SaveData)

kSerializeVar(mlType, eSerializeType_Int32)
kSerializeVar(mfTime, eSerializeType_Float32)
kSerializeVar(mfCount, eSerializeType_Float32)

kSerializeVar(mvCustomValue, eSerializeType_Vector3f)
kSerializeVar(mfCustomValue, eSerializeType_Float32)
kSerializeVar(mlCustomValue, eSerializeType_Int32)

kSerializeVar(mlState, eSerializeType_Int32)

kEndSerialize()

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxEnemyPatrolNode_SaveData)

kSerializeVar(mlNodeId, eSerializeType_Int32)
kSerializeVar(mfWaitTime, eSerializeType_Float32)
kSerializeVar(msAnimation, eSerializeType_String)
kSerializeVar(mbLoopAnimation, eSerializeType_Bool)

kEndSerialize()

//-----------------------------------------------------------------------

kBeginSerializeVirtual(iLuxEnemy_SaveData, iLuxEntity_SaveData)

kSerializeVar(msFileName, eSerializeType_String)
kSerializeVar(m_mtxOnLoadTransform, eSerializeType_Matrixf)
kSerializeVar(mvOnLoadScale, eSerializeType_Vector3f)

kSerializeVar(mbDisabled, eSerializeType_Bool)
kSerializeVar(mbDisableTriggers, eSerializeType_Bool)

kSerializeVar(mfHealth, eSerializeType_Float32)
kSerializeVar(mbCausesSanityDecrease, eSerializeType_Bool)
kSerializeVarContainer(mvRagdollBodyMatrices, eSerializeType_Matrixf)

kSerializeVar(mbHallucination, eSerializeType_Bool)

kSerializeVar(mlCurrentState, eSerializeType_Int32)
kSerializeVar(mlNextState, eSerializeType_Int32)
kSerializeVar(mlPreviousState, eSerializeType_Int32)

kSerializeVar(mlSoundState, eSerializeType_Int32)

kSerializeVar(mfLookForPlayerCount, eSerializeType_Float32)
kSerializeVar(mlPlayerInLOSCount, eSerializeType_Int32)
kSerializeVar(mbCanSeePlayer, eSerializeType_Bool)
kSerializeVar(mbPlayerDetected, eSerializeType_Bool)
kSerializeVar(mbPlayerInRange, eSerializeType_Bool)

kSerializeVar(mfCheckAtDoorCount, eSerializeType_Float32)
kSerializeVar(mbStuckAtDoor, eSerializeType_Bool)
kSerializeVar(mlStuckDoorID, eSerializeType_Int32)

kSerializeVar(mfForwardSpeed, eSerializeType_Float32)
kSerializeVar(mfBackwardSpeed, eSerializeType_Float32)
kSerializeVar(mfForwardAcc, eSerializeType_Float32)
kSerializeVar(mfForwardDeacc, eSerializeType_Float32)

kSerializeVar(mfDarknessGlowAlpha, eSerializeType_Float32)
kSerializeVar(mfDarknessGlowAlphaGoal, eSerializeType_Float32)

kSerializeVar(mlAttackHitCounter, eSerializeType_Int32)

kSerializeVar(mfFOVMul, eSerializeType_Float32)

kSerializeVar(msCurrentAnimName, eSerializeType_String)
kSerializeVar(mbAnimationIsSpeedDependant, eSerializeType_Bool)
kSerializeVar(mfAnimationSpeedMul, eSerializeType_Float32)
kSerializeVar(mbUseMoveAnimWhenCurrentIsOver, eSerializeType_Bool)

kSerializeVar(mvStartPosition, eSerializeType_Vector3f)

kSerializeVar(mvLastKnownPlayerPos, eSerializeType_Vector3f)
kSerializeVar(mfLastPlayerPosCount, eSerializeType_Float32)

kSerializeVar(mvTempPos, eSerializeType_Vector3f)
kSerializeVar(mfTempVal, eSerializeType_Float32)
kSerializeVar(mlTempVal, eSerializeType_Int32)
kSerializeVar(mfDamageCount, eSerializeType_Float32)

kSerializeClassContainer(mvPatrolNodes, cLuxEnemyPatrolNode_SaveData, eSerializeType_Class)
kSerializeVar(mlCurrentPatrolNode, eSerializeType_Int32)

kSerializeVar(mMeshEntity, eSerializeType_Class)
kSerializeClassContainer(mvPS, cEnginePS_SaveData, eSerializeType_Class)
kSerializeClassContainer(mvBeams, cEngineBeam_SaveData, eSerializeType_Class)
kSerializeClassContainer(mvLights, cEngineLight_SaveData, eSerializeType_Class)
kSerializeClassContainer(mvBillboards, cEngineBillboard_SaveData, eSerializeType_Class)

kSerializeClassContainer(mvMessages, cLuxStateMessage_SaveData, eSerializeType_Class)

kSerializeVar(mMover, eSerializeType_Class)
kSerializeVar(mPathfinder, eSerializeType_Class)

kSerializeVar(mCharBody, eSerializeType_Class)

kEndSerialize()

//-----------------------------------------------------------------------

iLuxEnemy_SaveData::~iLuxEnemy_SaveData()
{
}

//-----------------------------------------------------------------------

cEnginePS_SaveData* iLuxEnemy_SaveData::GetParticleSystem(cParticleSystem* apPS)
{
	for(size_t i=0; i<mvPS.Size();++i)
		if(mvPS[i].msName == apPS->GetName()) return &mvPS[i];

	return NULL;
}

//-----------------------------------------------------------------------

iLuxEntity* iLuxEnemy_SaveData::CreateEntity(cLuxMap *apMap)
{
	cWorld *pWorld = apMap->GetWorld();
	apMap->ResetLatestEntity();
	pWorld->CreateEntity(msName,m_mtxOnLoadTransform, msFileName, mlID, true, mvOnLoadScale);

	return apMap->GetLatestEntity();
}

//-----------------------------------------------------------------------

void iLuxEnemy::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::SaveToSaveData(apSaveData);
	iLuxEnemy_SaveData *pData = static_cast<iLuxEnemy_SaveData*>(apSaveData);

    cWorld *pWorld = mpMap->GetWorld();
	iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();

	///////////////////////
	//Properties
	kCopyToVar(pData, msFileName);
	kCopyToVar(pData, m_mtxOnLoadTransform);
	kCopyToVar(pData, mvOnLoadScale);

	kCopyToVar(pData, mbDisabled);
	kCopyToVar(pData, mbDisableTriggers);

	kCopyToVar(pData, mfHealth);
	kCopyToVar(pData, mbCausesSanityDecrease);

	///////////////////////
	//Ragdoll pose
	//
	//The bodies are built at runtime, so their transforms are the only record of
	//where a corpse actually came to rest. Without this a loaded save rebuilds
	//from the bind pose and the body falls over again in front of the players.
	pData->mvRagdollBodyMatrices.Clear();
	if(mbRagdollActive && mpMeshEntity)
	{
		int lBoneNum = mpMeshEntity->GetBoneStateNum();
		pData->mvRagdollBodyMatrices.Resize(lBoneNum);

		for(int i=0; i<lBoneNum; ++i)
		{
			cBoneState *pState = mpMeshEntity->GetBoneState(i);
			iPhysicsBody *pBody = pState ? pState->GetBody() : NULL;

			//Bones without a body keep their slot so the vector stays index-aligned
			//with GetBoneState(); the value is never read back.
			pData->mvRagdollBodyMatrices[i] = pBody ? pBody->GetWorldMatrix()
												   : cMatrixf::Identity;
		}
	}

	kCopyToVar(pData, mbHallucination);
	
	pData->mlCurrentState = mCurrentState;
	pData->mlNextState = mNextState;
	pData->mlPreviousState = mPreviousState;

	pData->mlSoundState = mSoundState;

	kCopyToVar(pData, mfLookForPlayerCount);
	kCopyToVar(pData, mlPlayerInLOSCount);
	kCopyToVar(pData, mbCanSeePlayer);
	kCopyToVar(pData, mbPlayerDetected);
	kCopyToVar(pData, mbPlayerInRange);

	kCopyToVar(pData, mfCheckAtDoorCount);
	kCopyToVar(pData, mbStuckAtDoor);
	kCopyToVar(pData, mlStuckDoorID);

	kCopyToVar(pData, mfForwardSpeed);
	kCopyToVar(pData, mfBackwardSpeed);
	kCopyToVar(pData, mfForwardAcc);
	kCopyToVar(pData, mfForwardDeacc);

	kCopyToVar(pData, mfDarknessGlowAlpha);
	kCopyToVar(pData, mfDarknessGlowAlphaGoal);

	kCopyToVar(pData, mlAttackHitCounter);

	kCopyToVar(pData,mfFOVMul);

	kCopyToVar(pData, mbAnimationIsSpeedDependant);
	kCopyToVar(pData, mfAnimationSpeedMul);
	kCopyToVar(pData, mbUseMoveAnimWhenCurrentIsOver);

	kCopyToVar(pData, mvStartPosition);

	kCopyToVar(pData, mvLastKnownPlayerPos);
	kCopyToVar(pData, mfLastPlayerPosCount);

	kCopyToVar(pData, mvTempPos);
	kCopyToVar(pData, mfTempVal);
	kCopyToVar(pData, mlTempVal);
	kCopyToVar(pData, mfDamageCount);

	kCopyToVar(pData, mlCurrentPatrolNode);

	///////////////////////
	//Patrol nodes
	pData->mvPatrolNodes.Resize(mvPatrolNodes.size());
	for(size_t i=0; i<mvPatrolNodes.size(); ++i)
	{
		pData->mvPatrolNodes[i].mlNodeId = 	mvPatrolNodes[i].mpNode->GetID();
		pData->mvPatrolNodes[i].mfWaitTime = 	mvPatrolNodes[i].mfWaitTime;
		pData->mvPatrolNodes[i].msAnimation = 	mvPatrolNodes[i].msAnimation;
		pData->mvPatrolNodes[i].mbLoopAnimation = mvPatrolNodes[i].mbLoopAnimation;
	}
	
	///////////////////////
	//Current animation
	if(mpCurrentAnimation)
		pData->msCurrentAnimName = mpCurrentAnimation->GetName();
	else
		pData->msCurrentAnimName = "";

	///////////////////////
	//Mesh Entity
	if(mpMeshEntity)
	{
		pData->mMeshEntity.FromMeshEntity(mpMeshEntity);
	}

	
	///////////////////////
	//Particle Systems
	pData->mvPS.Resize(mvParticleSystems.size());
	for(size_t i=0; i<mvParticleSystems.size(); ++i)
	{
		if(pWorld->ParticleSystemExists(mvParticleSystems[i])==false)
		{
			mvParticleSystems[i] = NULL;
			Warning("particle system %d in %s does not exist anymore!\n",i,GetName().c_str());
		}

		pData->mvPS[i].FromPS(mvParticleSystems[i]);
	}
	
	///////////////////////
	//Lights
	pData->mvLights.Resize(mvLights.size());
	for(size_t i=0; i<mvLights.size(); ++i)
	{
		pData->mvLights[i].FromLight(mvLights[i]);
	}
	
	///////////////////////
	//Billboards
	pData->mvBillboards.Resize(mvBillboards.size());
	for(size_t i=0; i<mvBillboards.size(); ++i)
	{
		pData->mvBillboards[i].FromBillboard(mvBillboards[i]);
	}

	///////////////////////
	//Messages
	for(tLuxStateMessageListIt it = mlstMessages.begin(); it != mlstMessages.end(); ++it)
	{
		cLuxStateMessage& message = *it;
		cLuxStateMessage_SaveData savedMessage;

		savedMessage.mfCount = message.mfCount;
		savedMessage.mfTime = message.mfTime;
		savedMessage.mvCustomValue = message.mvCustomValue;
		savedMessage.mlState = message.mState;
		savedMessage.mlType = message.mType;
		
        pData->mvMessages.Add(savedMessage);	
	}

	///////////////////////
	//Helpers
	pData->mMover.FromMover(mpMover);
	pData->mPathfinder.FromPathfinder(mpPathfinder);

	///////////////////////
	//Character body
	if(mpCharBody)
	{
		pData->mCharBody.FromBody(mpCharBody);
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::LoadFromSaveData(apSaveData);
	iLuxEnemy_SaveData *pData = static_cast<iLuxEnemy_SaveData*>(apSaveData);

	cWorld *pWorld = mpMap->GetWorld();
	iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();

	///////////////////////
	//Properties
	kCopyFromVar(pData, m_mtxOnLoadTransform);
	kCopyFromVar(pData, mvOnLoadScale);

	kCopyFromVar(pData, mbDisabled);
	kCopyFromVar(pData, mbDisableTriggers);

	kCopyFromVar(pData, mfHealth);
	kCopyFromVar(pData, mbCausesSanityDecrease);

	//Held until the first update rebuilds the corpse -- the mesh entity and the
	//physics world are not guaranteed to be ready at this point.
	mvSavedRagdollPose.clear();
	mvSavedRagdollPose.reserve(pData->mvRagdollBodyMatrices.Size());
	for(size_t i=0; i<pData->mvRagdollBodyMatrices.Size(); ++i)
		mvSavedRagdollPose.push_back(pData->mvRagdollBodyMatrices[i]);

	kCopyFromVar(pData, mbHallucination);

	
	mCurrentState = (eLuxEnemyState)pData->mlCurrentState;
	mNextState = (eLuxEnemyState)pData->mlNextState;
	mPreviousState = (eLuxEnemyState)pData->mlPreviousState;

	ChangeSoundState((eLuxEnemySoundState)pData->mlSoundState);

	kCopyFromVar(pData, mfLookForPlayerCount);
	kCopyFromVar(pData, mlPlayerInLOSCount);
	kCopyFromVar(pData, mbCanSeePlayer);
	kCopyFromVar(pData, mbPlayerDetected);
	kCopyFromVar(pData, mbPlayerInRange);

	kCopyFromVar(pData, mfCheckAtDoorCount);
	kCopyFromVar(pData, mbStuckAtDoor);
	kCopyFromVar(pData, mlStuckDoorID);

	kCopyFromVar(pData, mfForwardSpeed);
	kCopyFromVar(pData, mfBackwardSpeed);
	kCopyFromVar(pData, mfForwardAcc);
	kCopyFromVar(pData, mfForwardDeacc);

	kCopyFromVar(pData, mfDarknessGlowAlpha);
	kCopyFromVar(pData, mfDarknessGlowAlphaGoal);

	kCopyFromVar(pData, mlAttackHitCounter);

	kCopyFromVar(pData,mfFOVMul);

	kCopyFromVar(pData, mbAnimationIsSpeedDependant);
	kCopyFromVar(pData, mfAnimationSpeedMul);
	kCopyFromVar(pData, mbUseMoveAnimWhenCurrentIsOver);

	kCopyFromVar(pData, mvStartPosition);

	kCopyFromVar(pData, mvLastKnownPlayerPos);
	kCopyFromVar(pData, mfLastPlayerPosCount);

	kCopyFromVar(pData, mvTempPos);
	kCopyFromVar(pData, mfTempVal);
	kCopyFromVar(pData, mlTempVal);
	kCopyFromVar(pData, mfDamageCount);

	kCopyFromVar(pData, mlCurrentPatrolNode);

	///////////////////////
	//Current animation
	if(pData->msCurrentAnimName != "")
	{
		mpCurrentAnimation = mpMeshEntity->GetAnimationStateFromName(pData->msCurrentAnimName);
	}
	else
	{
		mpCurrentAnimation = NULL;
	}
	
	///////////////////////
	//Mesh Entity
	if(mpMeshEntity)
	{
		pData->mMeshEntity.ToMeshEntity(mpMeshEntity);
	}

	///////////////////////
	//Lights
	for(size_t i=0; i<mvLights.size(); ++i)
	{
		pData->mvLights[i].ToLight(mvLights[i]);
	}
	
	///////////////////////
	//Particle Systems
	int lCount=0;
	for(std::vector<cParticleSystem*>::iterator it = mvParticleSystems.begin(); it != mvParticleSystems.end();)
	{
		cParticleSystem *pPS = *it;

		cEnginePS_SaveData *pSavePS = pData->GetParticleSystem(pPS);
		if(pSavePS)
		{
			pSavePS->ToPS(pPS);
			++it;
		}
		else
		{
			//check if a null was previously saved
			if(pData->mvPS.Size() == mvParticleSystems.size() && pData->mvPS[lCount].msType == "")
			{
				++it;
			}
			//a particle system has been removed.
			else
			{
				pWorld->DestroyParticleSystem(pPS);
				it = mvParticleSystems.erase(it);
			}
		}

		++lCount;
	}

	///////////////////////
	//Billboards
	for(size_t i=0; i<mvBillboards.size(); ++i)
	{
		pData->mvBillboards[i].ToBillboard(mvBillboards[i]);
	}

	///////////////////////
	//Messages
	for(size_t i=0; i<pData->mvMessages.Size(); ++i)
	{
		cLuxStateMessage message;
		cLuxStateMessage_SaveData& savedMessage = pData->mvMessages[i];

		message.mfCount =		savedMessage.mfCount;
		message.mfTime =		savedMessage.mfTime;
		message.mvCustomValue = savedMessage.mvCustomValue;
		message.mState =		(eLuxEnemyState)savedMessage.mlState;
		message.mType =			(eLuxEnemyMessage)savedMessage.mlType;

		mlstMessages.push_back(message);	
	}

	///////////////////////
	//Helpers
	pData->mMover.ToMover(mpMover);
	pData->mPathfinder.ToPathfinder(mpPathfinder);

	///////////////////////
	//Character body
	if(mpCharBody)
	{
		pData->mCharBody.ToBody(mpCharBody);
	}
}

//-----------------------------------------------------------------------

void iLuxEnemy::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
	iLuxEnemy_SaveData *pData = static_cast<iLuxEnemy_SaveData*>(apSaveData);

	cAINodeContainer *pNodeContainer =  mpPathfinder->GetNodeContainer();

	///////////////////////
	//Patrol nodes
	if(pNodeContainer)
	{
		mvPatrolNodes.reserve(pData->mvPatrolNodes.Size());
		for(size_t i=0; i<pData->mvPatrolNodes.Size(); ++i)
		{
			cLuxEnemyPatrolNode patrolNode;

			patrolNode.mpNode =pNodeContainer->GetNodeFromID(pData->mvPatrolNodes[i].mlNodeId);
			if(patrolNode.mpNode==NULL)
			{
				Error("Could not find patrol node id: %d for '%s'!\n", pData->mvPatrolNodes[i].mlNodeId, msName.c_str());
				continue;
			}

			patrolNode.mfWaitTime = pData->mvPatrolNodes[i].mfWaitTime;
			patrolNode.msAnimation = pData->mvPatrolNodes[i].msAnimation;
			patrolNode.mbLoopAnimation = pData->mvPatrolNodes[i].mbLoopAnimation;

			mvPatrolNodes.push_back(patrolNode);
		}
	}

	///////////////////////
	//Helpers
	pData->mPathfinder.SetupPathfinder(mpPathfinder);
}

//-----------------------------------------------------------------------

