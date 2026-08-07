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

#include "LuxPlayerAvatar.h"

#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"
#include "LuxMap.h"
#include "LuxMapHandler.h"

#include "impl/ImGuiDebugMenu.h"

#include "scene/Scene.h"
#include "graphics/BoneState.h"
#include "graphics/Skeleton.h"
#include "graphics/Bone.h"
#include "graphics/Mesh.h"
#include "resources/MeshManager.h"

#include <math.h>
#include <stdio.h>

//////////////////////////////////////////////////////////////////////////
// AVATAR DIMENSIONS
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// RIGGED CHARACTER MODELS
//////////////////////////////////////////////////////////////////////////

/**
 * One real Amnesia character rig, mapped onto the joints the solver drives.
 *
 * The five rigs are the same skeleton under four different naming conventions, so
 * there is no way around a table. A NULL in a limb's [1] or [2] slot means "the
 * first child of the previous bone" -- the grunt and the brute leave their knees
 * and feet as jointNN, and walking the hierarchy is sturdier than pinning a bone
 * by a number that only holds for one export.
 */
class cLuxAvatarRigDef
{
public:
	const char *msName;
	const char *msMesh;

	const char *msRoot;			// pelvis
	const char *msSpine;		// pelvis-to-chest bone
	const char *msSpineTip;		// what it points at (neck or head)
	const char *msChest;		// optional extra torso bone; NULL on most rigs
	const char *msHead;

	const char *msArmL[3];		// shoulder, elbow, hand
	const char *msArmR[3];
	const char *msLegL[3];		// hip, knee, foot
	const char *msLegR[3];

	// Degrees of yaw between the way the rig faces and the way the character body
	// faces. Grunt and brute state theirs in their .ent as Body_OffsetRot 0 180 0,
	// which is what iLuxEnemy feeds to SetEntityOffset. The other three are
	// Object/NPC entities placed by map transform, carry no such var, and are
	// seeded at 180 on the assumption they share the convention -- use the debug
	// menu's Model Yaw Trim to find the truth, then put it here.
	float mfYaw;
};

//The brute reuses the grunt's rig VERBATIM -- same servant_grunt_A_ prefix on
//every bone -- so the two entries differ only by mesh.
#define kGruntBones \
	"servant_grunt_A_CenterRoot", "servant_grunt_A_CenterSpine", \
	"servant_grunt_A_CenterNeck", NULL, "servant_grunt_A_CenterHead", \
	{ "servant_grunt_A_LeftShoulder",  "servant_grunt_A_LeftElbow",  "servant_grunt_A_LeftHand" }, \
	{ "servant_grunt_A_RightShoulder", "servant_grunt_A_RightElbow", "servant_grunt_A_RightHand" }, \
	{ "servant_grunt_A_LeftHip",  NULL, NULL }, \
	{ "servant_grunt_A_RightHip", NULL, NULL }, 180.0f

static const cLuxAvatarRigDef gvAvatarRigs[] =
{
	//Index 0 is the stickman and has no rig; the entry keeps the array aligned
	//with eAvatarModel so a lookup is just gvAvatarRigs[model].
	{ "Stickman", NULL, NULL, NULL, NULL, NULL, NULL, {NULL,NULL,NULL}, {NULL,NULL,NULL}, {NULL,NULL,NULL}, {NULL,NULL,NULL}, 0.0f },

	{ "Servant Grunt", "entities/enemy/servant_grunt/servant_grunt.dae", kGruntBones },
	{ "Servant Brute", "entities/enemy/servant_brute/servant_brute.dae", kGruntBones },

	//Agrippa's head bone is CenterNeck_CenterHead -- the exporter folded the parent
	//name in. Not a typo.
	{ "Agrippa", "entities/character/agrippa/agrippa.dae",
		"CenterRoot", "CenterSpine", "CenterNeck", NULL, "CenterNeck_CenterHead",
		{ "LeftShoulder",  "LeftElbow",  "LeftHand" },
		{ "RightShoulder", "RightElbow", "RightHand" },
		{ "LeftHip",  "LeftKnee",  "LeftFoot" },
		{ "RightHip", "RightKnee", "RightFoot" }, 180.0f },

	{ "Alexander", "entities/character/alexander/alexander.dae",
		"center_root", "center_spine", "center_neck", NULL, "center_head",
		{ "left_shoulder",  "left_elbow",  "left_hand" },
		{ "right_shoulder", "right_elbow", "right_hand" },
		{ "left_hip",  "left_knee",  "left_foot" },
		{ "right_hip", "right_knee", "right_foot" }, 180.0f },

	//No CenterNeck on this rig at all, so the spine points straight at the head.
	//The only rig of the five with a CenterChest, and the only one whose bind pose
	//is not a standing figure -- so it is also the only one that needs its pelvis
	//and chest driven rather than left as authored.
	{ "Ritual Prisoner", "entities/character/ritual_prisoner/ritual_prisoner.dae",
		"CenterRoot", "CenterSpine", "CenterHead", "CenterChest", "CenterHead",
		{ "LeftShoulder",  "LeftElbow",  "LeftHand" },
		{ "RightShoulder", "RightElbow", "RightHand" },
		{ "LeftHip",  "LeftKnee",  "LeftFoot" },
		{ "RightHip", "RightKnee", "RightFoot" }, 180.0f },
};

//-----------------------------------------------------------------------

static cBone* gAvatarFirstChild(cBone *apBone)
{
	if(apBone==NULL) return NULL;
	cBoneIterator it = apBone->GetChildIterator();
	if(it.HasNext()) return it.Next();
	return NULL;
}

static cBone* gAvatarResolveBone(cSkeleton *apSkel, cBone *apPrev, const char *asName)
{
	if(asName && asName[0]) return apSkel->GetBoneByName(asName);
	return gAvatarFirstChild(apPrev);
}

/** Shortest rotation taking one unit vector onto another. */
static cMatrixf gAvatarRotateFromTo(const cVector3f &avFrom, const cVector3f &avTo)
{
	float fDot = cMath::Vector3Dot(avFrom, avTo);
	if(fDot > 0.99999f) return cMatrixf::Identity;

	cVector3f vAxis;
	if(fDot < -0.99999f)
	{
		//Exactly opposed: the cross is degenerate, so any perpendicular will do.
		vAxis = cMath::Vector3Cross(avFrom, cVector3f(0,1,0));
		if(vAxis.SqrLength() < 0.0001f) vAxis = cMath::Vector3Cross(avFrom, cVector3f(1,0,0));
	}
	else
	{
		vAxis = cMath::Vector3Cross(avFrom, avTo);
	}

	if(vAxis.SqrLength() < 0.0000001f) return cMatrixf::Identity;
	vAxis.Normalize();

	float fAngle = acosf(cMath::Clamp(fDot, -1.0f, 1.0f));
	return cMath::MatrixQuaternion(cQuaternion(fAngle, vAxis));
}

//-----------------------------------------------------------------------

/**
 * The normal of a two-bone limb's plane.
 *
 * This is the vector that pins a limb's ROLL, and it is the right one because it
 * is exactly perpendicular to BOTH bones -- both of them lie in the plane, so its
 * normal is perpendicular to each. Always, in every pose, by construction.
 *
 * The POLE is not that vector, which is what was handed in before. A pole is
 * perpendicular to the root-to-tip line, not to either bone, so flattening it
 * against each bone in turn throws away a different amount for the upper arm than
 * for the forearm and the two come out rolled by different angles -- and where a
 * bone lines up with the pole it collapses to noise and the roll follows the noise.
 * Measured on the real rigs, that was a 179 degree flip of the forearm for one
 * degree of camera pitch, in the lantern, grab and gun poses alike.
 *
 * Built from the same two vectors SolveIK uses to place the middle joint, matching
 * fallback included, so the plane reported here is the plane the joint was actually
 * placed in rather than a second opinion about it.
 *
 * Zero if there is no limb to speak of, which PointBone reads as "do not roll".
 */
static cVector3f gAvatarLimbNormal(const cVector3f &avRoot, const cVector3f &avTip,
								   const cVector3f &avPole)
{
	cVector3f vAxis = avTip - avRoot;
	if(vAxis.SqrLength() < 0.0000001f) return cVector3f(0,0,0);
	vAxis.Normalize();

	cVector3f vPole = avPole - vAxis * cMath::Vector3Dot(avPole, vAxis);

	//SolveIK's own threshold and fallback, deliberately duplicated: if it placed the
	//joint using a substitute pole, the plane has to be measured with that same
	//substitute or the roll describes a plane the limb is not in.
	if(vPole.SqrLength() < 0.000001f)
	{
		vPole = cMath::Vector3Cross(vAxis, cVector3f(0,1,0));
		if(vPole.SqrLength() < 0.000001f) vPole = cMath::Vector3Cross(vAxis, cVector3f(1,0,0));
	}
	if(vPole.SqrLength() < 0.0000001f) return cVector3f(0,0,0);
	vPole.Normalize();

	cVector3f vNorm = cMath::Vector3Cross(vAxis, vPole);
	if(vNorm.SqrLength() < 0.0000001f) return cVector3f(0,0,0);
	vNorm.Normalize();

	return vNorm;
}

//-----------------------------------------------------------------------

//The sign I could not settle from the files, now settled by looking at it: with
//+1 the head pitched opposite to the look. -1 it is.
static const float gfAvatarHeadPitchSign = -1.0f;

static const float gfAvatarHeadRadius = 0.17f;
// Wig. The shell sits just proud of the scalp; the drops are fractions of a
// quarter turn from the crown, so 1.0 reaches the equator and above that it
// hangs. Front stops well clear of the nose, back falls to about jaw height.
static const float gfAvatarHairScale = 1.06f;
static const float gfAvatarHairFrontDrop = 0.55f;
static const float gfAvatarHairBackDrop = 1.55f;
static const float gfAvatarNoseRadius = 0.06f;
static const float gfAvatarNoseLength = 0.17f;
// Nose base sits slightly inside the head sphere so they connect seamlessly.
static const float gfAvatarNoseZOffset = -0.12f;

static const float gfLimbThickness = 0.045f;
static const float gfTorsoThickness = 0.06f;

static const float gfShoulderHalfWidth = 0.16f;
static const float gfHipHalfWidth = 0.09f;

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerAvatar::cLuxPlayerAvatar(cLuxPlayer *apPlayer, const cColor &aColor)
{
	mpPlayer = apPlayer;
	mColor = aColor;

	for(int i=0; i<eAvatarEntity_LastEnum; ++i) mvEntities[i] = NULL;

	mlRigModel = eAvatarModel_Stickman;
	mlFailedRigModel = -1;
	mpRigMesh = NULL;
	mpRigEntity = NULL;
	for(int i=0; i<eRigBone_LastEnum; ++i)
	{
		mvRigBones[i] = NULL;
		mvRigBind[i] = NULL;
		mvRigBindDir[i] = cVector3f(0,1,0);
		mvRigBindRef[i] = cVector3f(0,0,0);
	}
	mvRigFoot[0] = NULL;
	mvRigFoot[1] = NULL;
	for(int i=0; i<2; ++i)
	{
		mfRigHipY[i] = 0;
		mfRigThighLen[i] = 0;
		mfRigShinLen[i] = 0;
		mfRigFootYBind[i] = 0;
	}
	mpEntityWorld = NULL;
	mbEntitiesVisible = false;
	mbHoldingWorldObject = false;

	mfGaitPhase = 0;
	mfSpeed = 0;
	mvMoveDir = cVector3f(0, 0, -1);

	tString sName = mpPlayer->IsPlayer2() ? "PlayerAvatarP2" : "PlayerAvatarP1";

	////////////////////////////////////////////
	// Solid color texture (4x4) for the material.
	iLowLevelGraphics *pLowGfx = gpBase->mpEngine->GetGraphics()->GetLowLevel();

	mpColorTexture = pLowGfx->CreateTexture(sName + "_Color", eTextureType_2D, eTextureUsage_Normal);

	unsigned char vPixels[4*4*4];
	unsigned char lR = (unsigned char)(mColor.r * 255.0f);
	unsigned char lG = (unsigned char)(mColor.g * 255.0f);
	unsigned char lB = (unsigned char)(mColor.b * 255.0f);
	for(int i=0; i<4*4; ++i)
	{
		vPixels[i*4+0] = lR;
		vPixels[i*4+1] = lG;
		vPixels[i*4+2] = lB;
		vPixels[i*4+3] = 255;
	}
	// NOTE: z must be 1 — the GL upload size is computed as x*y*z*bpp, so a
	// z of 0 uploads ZERO bytes and the texture samples black (invisible
	// avatars in a dark game).
	mpColorTexture->CreateFromRawData(cVector3l(4, 4, 1), ePixelFormat_RGBA, vPixels);

	////////////////////////////////////////////
	// The wig's own colour. Not pure black: with nothing but a lantern in the
	// room, 0,0,0 is a hole in the screen with no shading on it at all. This
	// still catches the light and reads as black hair.
	mpHairTexture = pLowGfx->CreateTexture(sName + "_HairColor", eTextureType_2D, eTextureUsage_Normal);

	unsigned char vHairPixels[4*4*4];
	for(int i=0; i<4*4; ++i)
	{
		vHairPixels[i*4+0] = 18;
		vHairPixels[i*4+1] = 16;
		vHairPixels[i*4+2] = 18;
		vHairPixels[i*4+3] = 255;
	}
	mpHairTexture->CreateFromRawData(cVector3l(4, 4, 1), ePixelFormat_RGBA, vHairPixels);

	////////////////////////////////////////////
	// A proper soliddiffuse material, so the deferred renderer lights the
	// avatar like any world prop (and it shows up in water reflections).
	cResources *pResources = gpBase->mpEngine->GetResources();
	iMaterialType *pMatType = gpBase->mpEngine->GetGraphics()->GetMaterialType("soliddiffuse");

	mpMaterial = pResources->GetMaterialManager()->CreateCustomMaterial(sName + "_Mat", pMatType);
	mpMaterial->SetTexture(eMaterialTexture_Diffuse, mpColorTexture);
	mpMaterial->Compile();

	//One material, one submesh, so no IncUserCount balancing needed here --
	//CreateCustomMaterial starts the count at 1 and the wig's submesh releases 1.
	mpHairMaterial = pResources->GetMaterialManager()->CreateCustomMaterial(sName + "_HairMat", pMatType);
	mpHairMaterial->SetTexture(eMaterialTexture_Diffuse, mpHairTexture);
	mpHairMaterial->Compile();

	////////////////////////////////////////////
	// Meshes. Each submesh's destructor releases one material reference, so
	// balance the user count: CreateCustomMaterial started it at 1, and we
	// hand it to 3 submeshes.
	mpMaterial->IncUserCount();
	mpMaterial->IncUserCount();

	mpLimbMesh = CreateMesh(sName + "_Limb", CreateCylinderVB(8));
	mpHeadMesh = CreateMesh(sName + "_Head", CreateSphereVB(16, 10, gfAvatarHeadRadius));
	mpNoseMesh = CreateMesh(sName + "_Nose", CreateConeVB(16, gfAvatarNoseRadius, gfAvatarNoseLength, gfAvatarNoseZOffset));
	mpHairMesh = CreateMesh(sName + "_Hair", CreateHairVB(18, 8, gfAvatarHeadRadius), mpHairMaterial);

	//The carried gun. Reuses the limb cylinder and the wig's near-black material
	//rather than adding a mesh and a material of its own -- SetSegmentMatrix
	//already stretches that cylinder between two points, which is all a gun stub
	//needs. The hair material now feeds TWO submeshes, so it needs one more
	//reference or the second one to die takes it below zero.
	mpHairMaterial->IncUserCount();
	mpGunMesh = CreateMesh(sName + "_Gun", CreateCylinderVB(8), mpHairMaterial);

	// Our own reference on each mesh, so entity destruction (which
	// DecUserCounts the mesh) never deletes them between maps.
	mpLimbMesh->IncUserCount();
	mpHeadMesh->IncUserCount();
	mpNoseMesh->IncUserCount();
	mpHairMesh->IncUserCount();
	mpGunMesh->IncUserCount();
}

//-----------------------------------------------------------------------

cLuxPlayerAvatar::~cLuxPlayerAvatar()
{
	// NOTE: world entities are destroyed in DestroyWorldEntities (or by the
	// world itself at teardown) — never here. Releasing our mesh references
	// deletes the meshes, whose submeshes release the material, which in
	// turn destroys the color texture.
	cMeshManager *pMeshManager = gpBase->mpEngine->GetResources()->GetMeshManager();

	if(mpLimbMesh) pMeshManager->Destroy(mpLimbMesh);
	if(mpHeadMesh) pMeshManager->Destroy(mpHeadMesh);
	if(mpNoseMesh) pMeshManager->Destroy(mpNoseMesh);
	if(mpHairMesh) pMeshManager->Destroy(mpHairMesh);
	if(mpGunMesh)  pMeshManager->Destroy(mpGunMesh);

	//The rig ENTITY belongs to a world that may already be gone, so only the mesh
	//reference is released here -- same rule as the meshes above. DestroyRig on the
	//map-leave path is what removes the entity while its world is still alive.
	if(mpRigMesh)
	{
		pMeshManager->Destroy(mpRigMesh);
		mpRigMesh = NULL;
	}
	mpRigEntity = NULL;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerAvatar::AbandonStaleEntities()
{
	// Drop the pointers. Nothing else. Do not dereference them, do not destroy
	// them, and above all do not touch their mesh reference counts.
	//
	// This used to call pMesh->DecUserCount() once per entity, to "release" the
	// reference CreateWorldEntities takes. That was wrong, and it is what produced
	// the heap corruption in ~cMeshEntity at MeshEntity.cpp:277.
	//
	// The premise was that an abandoned entity is never destroyed, so its mesh
	// reference would leak. It is destroyed: the world owns it in
	// mlstDynamicMeshEntities, and cWorld::~cWorld -> DestroyAllEntities(0) ->
	// STLDeleteAll(mlstDynamicMeshEntities) runs every single destructor, each of
	// which releases its own reference through cMeshManager::Destroy. Releasing it
	// here as well is a SECOND release of the same reference -- and one that can
	// never delete, because DecUserCount() bypasses the manager and just clamps:
	//
	//     void DecUserCount(){ if(mlUserCount>0) mlUserCount--; }   ResourceBase.h:68
	//
	// Eleven of the fifteen parts share mpLimbMesh and we hold exactly ONE spare
	// reference on it, so one unbalanced decrement is the whole margin:
	//
	//   ctor                  Inc          ->  1
	//   CreateWorldEntities   Inc x11      -> 12
	//   this function         Dec x11      ->  1   (entities still alive!)
	//   cWorld::~cWorld       Destroy x11  ->  1st hits 0 and hplDeletes the mesh,
	//                                          the other 10 free freed memory.
	//
	// The clamp hides the underflow, which is why it surfaced as corruption inside
	// an unrelated destructor a whole map later instead of an assert here.
	//
	// Nothing leaks now: the only way an entity's destructor never runs is the
	// world never being destroyed, and then there was nothing to reclaim anyway.
	// cLuxPlayerGun abandons the same kind of entities the same way and has always
	// just dropped its pointers -- this was the one outlier in the codebase.
	for(int i=0; i<eAvatarEntity_LastEnum; ++i)
	{
		mvEntities[i] = NULL;
	}

	//Same rule for the rig, and for the same reason: the entity belongs to a world
	//that is already gone, so it must not be dereferenced, destroyed, or have its
	//mesh reference touched. Our OWN mesh reference (taken in EnsureRigForModel)
	//survives and is released in the destructor.
	mpRigEntity = NULL;
	for(int i=0; i<eRigBone_LastEnum; ++i)
	{
		mvRigBones[i] = NULL;
		mvRigBind[i] = NULL;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerAvatar::CreateWorldEntities(cLuxMap *apMap)
{
	cWorld *pWorld = apMap->GetWorld();

	// If our cached entities belong to a DIFFERENT world, that world (and the
	// entities in it) was already destroyed -- e.g. quit-to-menu calls DestroyMap()
	// directly, bypassing DestroyWorldEntities(). Abandon the stale pointers
	// WITHOUT touching them (already freed) so the loop below rebuilds fresh
	// entities instead of skipping them. Fixes a use-after-free crash in Update().
	if(mpEntityWorld != pWorld)
	{
		AbandonStaleEntities();
		mpEntityWorld = pWorld;
	}

	tString sName = mpPlayer->IsPlayer2() ? "PlayerAvatarP2" : "PlayerAvatarP1";

	for(int i=0; i<eAvatarEntity_LastEnum; ++i)
	{
		if(mvEntities[i]) continue;

		cMesh *pMesh = mpLimbMesh;
		if(i == eAvatarEntity_Head) pMesh = mpHeadMesh;
		else if(i == eAvatarEntity_Nose) pMesh = mpNoseMesh;
		else if(i == eAvatarEntity_Hair) pMesh = mpHairMesh;
		else if(i == eAvatarEntity_Gun)  pMesh = mpGunMesh;

		// Each entity's destructor releases one mesh reference.
		pMesh->IncUserCount();

		cMeshEntity *pEnt = pWorld->CreateMeshEntity(sName + "_" + cString::ToString(i), pMesh, false);
		pEnt->SetVisible(false);
		pEnt->SetRenderFlagBit(eRenderableFlag_ShadowCaster, false);

		mvEntities[i] = pEnt;
	}

	mbEntitiesVisible = false;

	EnsureRigForModel(pWorld);
}

//-----------------------------------------------------------------------

void cLuxPlayerAvatar::DestroyWorldEntities(cLuxMap *apMap)
{
	cWorld *pWorld = apMap->GetWorld();

	//Before the stale-world check below, because DestroyRig makes the same check
	//itself and this is the one path where the world is known to still be alive.
	if(mpEntityWorld == pWorld) DestroyRig();

	// If the entities belong to a different (already-destroyed) world, just drop		
	// the stale pointers -- never call DestroyMeshEntity on the wrong world.
	if(mpEntityWorld != pWorld)
	{
		AbandonStaleEntities();
		mpEntityWorld = NULL;
		return;
	}

	for(int i=0; i<eAvatarEntity_LastEnum; ++i)
	{
		if(mvEntities[i] == NULL) continue;

		pWorld->DestroyMeshEntity(mvEntities[i]);
		mvEntities[i] = NULL;
	}

	mpEntityWorld = NULL;
}

//-----------------------------------------------------------------------

void cLuxPlayerAvatar::SetEntitiesVisible(bool abX)
{
	mbEntitiesVisible = abX;

	// CRASH GUARD. This runs from the viewport pre-draw callback, so it can fire
	// in the window between a world being destroyed and this avatar being told
	// about it -- quit-to-menu and level changes destroy the map directly, and a
	// frame can still be rendered before CreateWorldEntities/DestroyWorldEntities
	// gets a chance to abandon the stale pointers. mvEntities then point into
	// freed memory and IsVisible() reads garbage.
	//
	// Two cheap tells that we are inside that window:
	//   - the player has no character body, i.e. its world entities are gone
	//   - our cached world is no longer registered with the scene
	// Either way the entities are stale, so touch nothing. The next
	// CreateWorldEntities abandons them properly and rebuilds.
	////////////////////////////////////////////////////////////////////////////
	// NO WORLD-GRAPH MUTATION HERE. This runs from
	// cLuxCoopAvatarVisCallback::OnPreWorldDraw -- INSIDE cScene::Render, between
	// the two co-op viewports, while the renderer walks the world's container.
	//
	// EnsureEntitiesForCurrentWorld() was called here. It reaches CreateWorldEntities
	// -> cWorld::CreateMeshEntity -> AddRenderableToContainer, which decrements
	// cRenderableContainer_DynBoxTree::mlRebuildCount. At zero the next
	// UpdateBeforeRendering flattens the tree and STLDeleteAll's every node -- and
	// that runs at the top of EACH viewport's cull pass. The per-viewport
	// cVisibleRCNodeTracker keys on raw node pointers, so the second viewport's
	// tracker then refers to freed nodes, CHC takes its not-previously-visible
	// branch, issues occlusion queries instead of draws, and that viewport renders
	// NOTHING.
	//
	// One viewport black for a frame or more, nothing visibly wrong in any state,
	// recovering by itself once the tree restabilises. Impossible in single player:
	// this callback exists only on the co-op viewports, and viewport callbacks never
	// ran at all in stock HPL2 -- cViewport::RunViewportCallbackMessage compared
	// against begin() instead of end(), so the loop body never executed.
	//
	// The repair still happens, once per frame from Update, where nothing is
	// iterating the container.

	//DIAGNOSTIC, read-only. Every early-out below reads differently here, so an
	//invisible player states which link broke instead of leaving it to be guessed.
	//Deliberately placed AFTER the repair above, so it reports the FINAL state.
	{
		cLuxMap *pDiagMap   = gpBase->mpMapHandler->GetCurrentMap();
		cWorld  *pDiagWorld = pDiagMap ? pDiagMap->GetWorld() : NULL;
		bool bWorldAlive    = mpEntityWorld != NULL &&
							  gpBase->mpEngine->GetScene()->WorldExists(mpEntityWorld);

		int lFilled = 0;
		for(int i=0; i<eAvatarEntity_LastEnum; ++i)
		{
			if(mvEntities[i] != NULL) ++lFilled;
		}

		char sDiag[256];
		snprintf(sDiag, sizeof(sDiag),
				 "world %p %s | mapworld %p | parts %d/%d | body %s | vis %d",
				 (void*)mpEntityWorld, bWorldAlive ? "alive" : "DEAD/NULL",
				 (void*)pDiagWorld, lFilled, (int)eAvatarEntity_LastEnum,
				 (mpPlayer && mpPlayer->GetCharacterBody()) ? "ok" : "NULL",
				 abX ? 1 : 0);

		ImGuiDebugMenu::SetDiagAvatarLine((mpPlayer && mpPlayer->IsPlayer2()) ? 1 : 0, sDiag);
	}

	if(mpPlayer == NULL || mpPlayer->GetCharacterBody() == NULL) return;
	if(mpEntityWorld == NULL) return;
	if(gpBase->mpEngine->GetScene()->WorldExists(mpEntityWorld) == false) return;

	// No early-out on mbEntitiesVisible: the debug menu part toggles (legs /
	// arms) can change at any moment, so per-part visibility is re-evaluated
	// on every call (this runs right before each viewport renders).
	for(int i=0; i<eAvatarEntity_LastEnum; ++i)
	{
		if(mvEntities[i] == NULL) continue;

		bool bShow = abX && PartEnabled((eAvatarEntity)i);
		if(mvEntities[i]->IsVisible() != bShow) mvEntities[i]->SetVisible(bShow);
	}

	//EnsureRigForModel is not called here either, for the same reason -- it destroys
	//and recreates a skinned mesh entity, the same container surgery. It runs from
	//Update; a debug-menu model swap takes effect one frame later.
	if(mpRigEntity && mpRigEntity->IsVisible() != abX) mpRigEntity->SetVisible(abX);
}

//-----------------------------------------------------------------------

/**
 * Is the player currently holding / manipulating a world object?
 *
 * Covers the physics grab (chairs, crates) plus every "hands are on it"
 * interaction state — doors, levers, wheels, sliders, pushables. Deliberately
 * excludes eLuxPlayerState_UseItem and _HandObject (an inventory item is not a
 * world object) and _Ladder (climbing is not holding).
 */
static bool IsHoldingWorldObject(eLuxPlayerState aState)
{
	switch(aState)
	{
	case eLuxPlayerState_InteractGrab:
	case eLuxPlayerState_InteractPush:
	case eLuxPlayerState_InteractSwingDoor:
	case eLuxPlayerState_InteractLever:
	case eLuxPlayerState_InteractWheel:
	case eLuxPlayerState_InteractSlide:
		return true;

	default:
		return false;
	}
}

//-----------------------------------------------------------------------

bool cLuxPlayerAvatar::GunIsUp()
{
	return mpPlayer && mpPlayer->GetHelperGun() && mpPlayer->GetHelperGun()->IsActive();
}

//-----------------------------------------------------------------------

void cLuxPlayerAvatar::EnsureEntitiesForCurrentWorld()
{
	if(mpPlayer == NULL || mpPlayer->GetCharacterBody() == NULL) return;

	//No map or no world yet: a load is in progress. Nothing to attach to, and
	//staying invisible until there is something is the correct answer.
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap == NULL) return;

	cWorld *pWorld = pMap->GetWorld();
	if(pWorld == NULL) return;
	if(gpBase->mpEngine->GetScene()->WorldExists(pWorld) == false) return;

	//ONLY the world we already belong to, and ONLY to fill gaps.
	//
	//Changing world is deliberately not handled here. CreateWorldEntities starts by
	//calling AbandonStaleEntities, which drops the pointers WITHOUT destroying the
	//entities. From the pre-draw callback that can fire in the window where
	//GetCurrentMap() is already the NEW map while a viewport still renders the OLD
	//one -- so it would forget entities the old world has not destroyed yet and
	//build a second set, leaving the avatar posed from two worlds at once until the
	//old one is torn down.
	//
	//(It is no longer a double-release: AbandonStaleEntities used to DecUserCount a
	//mesh per entity here, which was the heap corruption in ~cMeshEntity. It now
	//only clears pointers. The ordering hazard above stands on its own.)
	//
	//Filling empty slots is strictly additive and cannot release anything, and the
	//missing-parts case is the one this exists for. The world-changed case belongs
	//to the Create/DestroyWorldEntities pair, which runs where the ordering is known.
	if(mpEntityWorld != pWorld) return;

	bool bComplete = true;
	for(int i=0; i<eAvatarEntity_LastEnum; ++i)
	{
		if(mvEntities[i] == NULL) { bComplete = false; break; }
	}
	if(bComplete) return;

	//Skips every slot that is already filled, so this only ever adds.
	CreateWorldEntities(pMap);
}

//-----------------------------------------------------------------------

bool cLuxPlayerAvatar::PartEnabled(eAvatarEntity aEntity)
{
	//A real body is showing, so the stickman gets out of the way -- except the gun
	//stub, which is the "they are armed" tell and has no equivalent on the rig.
	if(RigActive())
		return aEntity == eAvatarEntity_Gun && GunIsUp();

	switch(aEntity)
	{
	case eAvatarEntity_HipBar:
	case eAvatarEntity_ThighL: case eAvatarEntity_ShinL:
	case eAvatarEntity_ThighR: case eAvatarEntity_ShinR:
		return ImGuiDebugMenu::GetCoopShowLegs();

	// The RIGHT arm is the "I'm holding something" tell, so it shows while
	// grabbing even when "Show Co-Op Arms" is off (which is the default) —
	// otherwise the whole indicator would be invisible for most players. The
	// shoulder bar comes along with it so the arm reads as attached to the torso
	// rather than floating beside it.
	case eAvatarEntity_UpperArmR: case eAvatarEntity_ForeArmR:
		return ImGuiDebugMenu::GetCoopShowArms() || mbHoldingWorldObject || GunIsUp();

	case eAvatarEntity_ShoulderBar:
		return ImGuiDebugMenu::GetCoopShowArms() || mbHoldingWorldObject || GunIsUp();

	//Only while it is actually raised -- that IS the tell.
	case eAvatarEntity_Gun:
		return GunIsUp();

	case eAvatarEntity_UpperArmL: case eAvatarEntity_ForeArmL:
		return ImGuiDebugMenu::GetCoopShowArms();

	// Justine only, and only when asked for.
	case eAvatarEntity_Hair:
		return mpPlayer && mpPlayer->IsPlayer2() && ImGuiDebugMenu::GetCoopP2Wig();

	default:
		return true;	// torso, head, nose — the cute floating stick
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerAvatar::Update(float afTimeStep)
{
	//Moved out of SetEntitiesVisible -- see the note there. Safe here: Update runs on
	//the logic tick, with nothing iterating the renderable container.
	EnsureEntitiesForCurrentWorld();
	if(mpEntityWorld) EnsureRigForModel(mpEntityWorld);

	cCamera *pCam = mpPlayer ? mpPlayer->GetCamera() : NULL;
	iCharacterBody *pCharBody = mpPlayer ? mpPlayer->GetCharacterBody() : NULL;
	if(pCam == NULL || pCharBody == NULL) return;
	if(mvEntities[0] == NULL) return;

	// Same stale-world window as SetEntitiesVisible -- see the note there.
	if(mpEntityWorld == NULL) return;
	if(gpBase->mpEngine->GetScene()->WorldExists(mpEntityWorld) == false) return;

	////////////////////////////////////////////
	// Horizontal velocity of the character body drives the gait.
	cVector3f vVel = pCharBody->GetVelocity(gpBase->mpEngine->GetStepSize());
	cVector3f vVelXZ(vVel.x, 0, vVel.z);
	float fSpeed = vVelXZ.Length();

	// Smooth the speed so stride/step-rate do not pop.
	float fSmooth = cMath::Clamp(afTimeStep * 10.0f, 0.0f, 1.0f);
	mfSpeed += (fSpeed - mfSpeed) * fSmooth;

	// Smooth the movement direction (only while actually moving, so feet
	// settle in place when stopping instead of snapping).
	if(fSpeed > 0.05f)
	{
		cVector3f vDir = vVelXZ / fSpeed;
		mvMoveDir += (vDir - mvMoveDir) * cMath::Clamp(afTimeStep * 12.0f, 0.0f, 1.0f);
		float fLen = mvMoveDir.Length();
		if(fLen > 0.001f) mvMoveDir = mvMoveDir / fLen;

		// Step rate scales with speed: slow walk = slow steps, sprint = fast.
		mfGaitPhase += afTimeStep * (3.5f + mfSpeed * 5.0f);
		if(mfGaitPhase > k2Pif * 1000.0f) mfGaitPhase -= k2Pif * 1000.0f;
	}

	////////////////////////////////////////////
	// Shared frame data
	cVector3f vCamPos = pCam->GetPosition();
	cVector3f vFeet = pCharBody->GetFeetPosition();

	float fCurHeight = pCharBody->GetSize().y;		// live collider height (crouch aware)
	float fStandHeight = mpPlayer->GetBodySize().y;	// standing height (bone lengths)

	// Horizontal facing vectors from the camera.
	cVector3f vFwd = pCam->GetForward();
	vFwd.y = 0;
	if(vFwd.Length() < 0.01f) vFwd = mvMoveDir;	// looking straight up/down
	vFwd.Normalize();
	cVector3f vRight = cMath::Vector3Cross(vFwd, cVector3f(0, 1, 0)) * -1.0f;
	vRight.Normalize();

	////////////////////////////////////////////
	// Skeleton anchors.
	// Movement lean: the hip joint trails BEHIND the head along the movement
	// direction, so the body tilts with the lower part backwards (top
	// leading into the walk) — for forward, backward and strafing alike.
	// The debug menu "Invert Body Tilt" toggle flips this the other way.
	cVector3f vLean = mvMoveDir * cMath::Clamp(mfSpeed * 0.045f, 0.0f, 0.16f);
	if(ImGuiDebugMenu::GetCoopInvertBodyTilt()) vLean = vLean * -1.0f;

	cVector3f vNeck = vCamPos - cVector3f(0, gfAvatarHeadRadius + 0.03f, 0);
	cVector3f vShoulderC = vNeck - cVector3f(0, 0.03f, 0);
	cVector3f vHip = vFeet + cVector3f(0, fCurHeight * 0.47f, 0) - vLean;

	cVector3f vShL = vShoulderC - vRight * gfShoulderHalfWidth;
	cVector3f vShR = vShoulderC + vRight * gfShoulderHalfWidth;
	cVector3f vHipL = vHip - vRight * gfHipHalfWidth;
	cVector3f vHipR = vHip + vRight * gfHipHalfWidth;

	// Foot anchors stay under the CHARACTER (un-leaned), so the lean tilts
	// the legs as well instead of shifting the whole figure.
	cVector3f vFootAnchorL = vFeet - vRight * gfHipHalfWidth;
	cVector3f vFootAnchorR = vFeet + vRight * gfHipHalfWidth;

	// Bone lengths from the STANDING height, so crouching bends the knees
	// (hip drops, legs stay the same length) instead of shrinking the legs.
	float fLegBone = fStandHeight * 0.53f * 0.5f;
	float fArmBone = fStandHeight * 0.36f * 0.5f;

	////////////////////////////////////////////
	// Gait: feet swing along the MOVEMENT direction (correct for strafing
	// and backpedaling), stride and lift scale with speed.
	// NOTE the minus sign: while a foot is LIFTED (sin phase > 0) it must
	// travel backward-to-forward relative to the movement direction — without
	// the negation the swing runs the other way and the walk reads inverted
	// ("moonwalking").
	float fStride = cMath::Clamp(mfSpeed * 0.13f, 0.0f, 0.40f);
	float fLift = cMath::Clamp(mfSpeed * 0.045f, 0.0f, 0.14f);

	float fSwingL = -cosf(mfGaitPhase);
	float fSwingR = -cosf(mfGaitPhase + kPif);
	float fLiftL = sinf(mfGaitPhase); if(fLiftL < 0) fLiftL = 0;
	float fLiftR = sinf(mfGaitPhase + kPif); if(fLiftR < 0) fLiftR = 0;

	cVector3f vFootL = cVector3f(vFootAnchorL.x, vFeet.y, vFootAnchorL.z) + mvMoveDir * (fSwingL * fStride) + cVector3f(0, fLiftL * fLift, 0);
	cVector3f vFootR = cVector3f(vFootAnchorR.x, vFeet.y, vFootAnchorR.z) + mvMoveDir * (fSwingR * fStride) + cVector3f(0, fLiftR * fLift, 0);

	// Knees bend towards facing-forward.
	cVector3f vKneeL = SolveIK(vHipL, vFootL, fLegBone, vFwd);
	cVector3f vKneeR = SolveIK(vHipR, vFootR, fLegBone, vFwd);

	////////////////////////////////////////////
	// Arms: swing opposite to the same-side leg and hang at the sides when idle.
	// Two poses override that hang, one per arm, so a player carrying a lit
	// lantern AND dragging a chair shows both arms out at once:
	//
	//   LEFT  hand -> the lantern carry position, while the lantern is lit.
	//   RIGHT hand -> pushed out in front, while holding a world object.
	//
	// The right arm is posed even when "Show Co-Op Arms" is off, because
	// PartEnabled() shows it on its own while holding — see there.
	mbHoldingWorldObject = IsHoldingWorldObject(mpPlayer->GetCurrentState());

	bool bShowArms = ImGuiDebugMenu::GetCoopShowArms();
	bool bGunUp = GunIsUp();

	cVector3f vHandL, vHandR, vElbowL, vElbowR;
	if(bShowArms || mbHoldingWorldObject || bGunUp)
	{
		float fArmSwing = fStride * 0.55f;
		float fHang = fArmBone * 2.0f * 0.92f;

		vHandL = vShL - vRight * 0.04f - cVector3f(0, fHang, 0)
					+ mvMoveDir * (fSwingR * fArmSwing);
		vHandR = vShR + vRight * 0.04f - cVector3f(0, fHang, 0)
					+ mvMoveDir * (fSwingL * fArmSwing);

		// Lantern pose: reach the LEFT hand to where the lantern carry model
		// hangs (in front, to the left, below the camera). Roughly — it does not
		// line up perfectly.
		cLuxPlayerLantern *pLantern = mpPlayer->GetHelperLantern();
		if(pLantern && pLantern->IsActive())
		{
			vHandL = vCamPos + pCam->GetForward() * 0.34f - vRight * 0.22f - cVector3f(0, 0.28f, 0);
		}

		// Holding pose: right hand out in front, a little further forward than the
		// lantern and closer to the centre line, so at split-screen distance the
		// silhouette clearly reads as "this player has hold of something".
		if(mbHoldingWorldObject)
		{
			vHandR = vCamPos + pCam->GetForward() * 0.46f + vRight * 0.15f - cVector3f(0, 0.20f, 0);
		}

		//Gun pose: right hand up and forward, roughly where the owner sees their own
		//view model, so the stub reads as aimed rather than dangling.
		if(bGunUp)
		{
			vHandR = vCamPos + pCam->GetForward() * 0.40f + vRight * 0.17f - cVector3f(0, 0.14f, 0);
		}

		// Elbows bend backwards and slightly outwards.
		vElbowL = SolveIK(vShL, vHandL, fArmBone, vFwd * -1.0f - vRight * 0.35f);
		vElbowR = SolveIK(vShR, vHandR, fArmBone, vFwd * -1.0f + vRight * 0.35f);
	}

	////////////////////////////////////////////
	// Rigged character model
	//
	// Same solved joints, a real skeleton instead of boxes. The model's own
	// animations are never used -- this IS the animation.
	if(RigActive())
	{
		// Place the whole entity first: the bind pose is expressed relative to it, and
		// every bone delta below is measured from there. Yaw only -- pitch belongs to
		// the head, and rolling the body with the camera would look drunk.
		// The rigs face the opposite way to the character body -- servant_grunt.ent
		// and servant_brute.ent both say Body_OffsetRot 0 180 0, which is what
		// iLuxEnemy hands to SetEntityOffset. Without this every model stood with its
		// back to where it was walking. The trim slider rides on top so the three
		// models whose .ent does not state a value can be dialled in live.
		float fRigYaw = pCharBody->GetYaw()
					  + cMath::ToRad(gvAvatarRigs[mlRigModel].mfYaw
								   + ImGuiDebugMenu::GetCoopModelYawTrim());

		cMatrixf mtxRig = cMath::MatrixRotateY(fRigYaw);
		mtxRig.SetTranslation(vFeet);
		mpRigEntity->SetMatrix(mtxRig);

		// Root-first, because each bone's local matrix is built against its parent's
		// world matrix and cNode3D resolves that lazily -- a parent written after its
		// child would silently invalidate the child.
		// DIRECTIONS only, never positions. The solver's joint positions come from the
		// PLAYER's proportions; forcing a real skeleton onto them rescales every bone
		// and squashes the model. Directions carry the pose without touching the size.
		//Pelvis first, then spine, then chest -- each bone's local matrix is built
		//against its parent's world matrix, so a parent written afterwards would
		//invalidate the child. All three aim along the torso.
		PointBone(eRigBone_Root, vNeck - vHip);
		PointBone(eRigBone_Spine, vNeck - vHip);
		PointBone(eRigBone_Chest, vNeck - vHip);

		// The head is PITCHED, not aimed.
		//
		// Aiming goes through PointBone's shortest-arc solve, and a shortest arc has
		// no opinion about ROLL -- it is whatever falls out of the axis. On a limb
		// that does not matter. On a head, roll is exactly the difference between
		// looking up and being upside down, which is what kept happening at full
		// look-up however the aim direction was built.
		//
		// PitchBone composes a pure rotation about a known axis onto the bind pose
		// instead. Nothing touches roll, so nothing can get it wrong, and it does not
		// care which way the head bone points in bind -- which matters, because the
		// ritual prisoner's points somewhere a standing character's never would.
		//
		// Clamped, because a neck is not a turret.
		const float fHeadLimit = cMath::ToRad(50.0f);
		PitchBone(eRigBone_Head, vRight,
				  cMath::Clamp(pCam->GetPitch(), -fHeadLimit, fHeadLimit) * gfAvatarHeadPitchSign);

		// Each limb hands over the normal of the plane it is bending in, which is what
		// stops its bones rolling freely about their own length. Aim alone leaves that
		// free: fine on a stickman's cylinder, very much not on a forearm holding a
		// lantern.
		//
		// The POLE went in here before -- the same hint SolveIK gets -- and it is not
		// perpendicular to the individual bones, only to the root-to-tip line. Rolling
		// against it rolled the upper arm and the forearm by different angles and
		// collapsed wherever a bone lined up with it. Sweeping the camera through the
		// real lantern, grab and gun poses that was a 179 degree flip of the forearm
		// for ONE degree of pitch, in all three. It is 2 to 3 degrees now.
		//
		// The end points are the ones SolveIK returned, not the ones it was asked for:
		// it clamps an out-of-reach target in place, and the plane has to be measured
		// where the limb actually is.
		cVector3f vNormArmL = gAvatarLimbNormal(vShL, vHandL, vFwd * -1.0f - vRight * 0.35f);
		cVector3f vNormArmR = gAvatarLimbNormal(vShR, vHandR, vFwd * -1.0f + vRight * 0.35f);
		cVector3f vNormLegL = gAvatarLimbNormal(vHipL, vFootL, vFwd);
		cVector3f vNormLegR = gAvatarLimbNormal(vHipR, vFootR, vFwd);

		PointBone(eRigBone_ShoulderL, vElbowL - vShL, vNormArmL);
		PointBone(eRigBone_ElbowL, vHandL - vElbowL, vNormArmL);
		PointBone(eRigBone_ShoulderR, vElbowR - vShR, vNormArmR);
		PointBone(eRigBone_ElbowR, vHandR - vElbowR, vNormArmR);

		PointBone(eRigBone_HipL, vKneeL - vHipL, vNormLegL);
		PointBone(eRigBone_KneeL, vFootL - vKneeL, vNormLegL);
		PointBone(eRigBone_HipR, vKneeR - vHipR, vNormLegR);
		PointBone(eRigBone_KneeR, vFootR - vKneeR, vNormLegR);

		// Plant the feet.
		//
		// Rotation-only posing bends the legs without shortening them, so crouching
		// swings the feet OUTWARD instead of bringing the body down -- the hips stay
		// at whatever height the bind pose puts them, and the model appears to float.
		// A rig whose origin is not at its feet is planted at the wrong height from
		// the start for the same reason, which is the ritual prisoner standing with
		// his torso at floor level.
		//
		// Worked out rather than measured. Reading it off the bone states was my
		// first attempt and it cannot work here: cNode3D world transforms are pushed
		// down the bone hierarchy by cMeshEntity::UpdateLogic, and this whole feature
		// depends on that block being SKIPPED. So GetWorldPosition returned a stale
		// model-space value near zero and the shift came out as a fixed sink of
		// vFeet.y, identical in every pose -- which is exactly how it looked.
		//
		// This uses only the bind pose and the directions handed to PointBone a few
		// lines above, so nothing can be stale:
		//
		//     footY = hipY_bind + dirThigh.y*thighLen + dirShin.y*shinLen
		//
		// The lengths are fixed because rotation-only posing never changes a bone's
		// length, and the hip's height above the model origin is fixed because
		// nothing on that branch above the hip is rotated.
		if(mfRigThighLen[0] > 0 && mfRigThighLen[1] > 0)
		{
			cVector3f vLegDir[4];
			vLegDir[0] = vKneeL - vHipL;	vLegDir[1] = vFootL - vKneeL;
			vLegDir[2] = vKneeR - vHipR;	vLegDir[3] = vFootR - vKneeR;
			for(int i=0; i<4; ++i)
			{
				if(vLegDir[i].SqrLength() > 0.0000001f) vLegDir[i].Normalize();
				else                                    vLegDir[i] = cVector3f(0,-1,0);
			}

			//How far each foot has RISEN from where the bind pose put it. Relative,
			//never absolute: the foot bone is the ankle and sits well above the sole, so
			//asking for footY == 0 buries the model by the height of its own foot, in
			//every pose. That was the remaining sink. Measuring from bind also stops
			//this caring where the rig puts its origin, since that cancels.
			float fRise0 = (mfRigHipY[0] + vLegDir[0].y*mfRigThighLen[0] + vLegDir[1].y*mfRigShinLen[0])
						 - mfRigFootYBind[0];
			float fRise1 = (mfRigHipY[1] + vLegDir[2].y*mfRigThighLen[1] + vLegDir[3].y*mfRigShinLen[1])
						 - mfRigFootYBind[1];

			//The foot that rose LEAST is the one still carrying the weight.
			float fRise = fRise0 < fRise1 ? fRise0 : fRise1;

			//Standing straight this is zero and the model sits exactly where it did
			//before any planting existed, which was right. Bend the legs and the body
			//drops by however far the foot came up, instead of the feet sliding out.
			mtxRig.SetTranslation(vFeet - cVector3f(0, fRise, 0));
			mpRigEntity->SetMatrix(mtxRig);
		}
	}

	////////////////////////////////////////////
	// Write entity transforms
	SetSegmentMatrix(eAvatarEntity_Torso, vNeck, vHip, gfTorsoThickness);
	SetSegmentMatrix(eAvatarEntity_HipBar, vHipL, vHipR, gfLimbThickness);

	SetSegmentMatrix(eAvatarEntity_ThighL, vHipL, vKneeL, gfLimbThickness);
	SetSegmentMatrix(eAvatarEntity_ShinL, vKneeL, vFootL, gfLimbThickness);
	SetSegmentMatrix(eAvatarEntity_ThighR, vHipR, vKneeR, gfLimbThickness);
	SetSegmentMatrix(eAvatarEntity_ShinR, vKneeR, vFootR, gfLimbThickness);

	// Mirrors the visibility rules in PartEnabled(): the shoulder bar and right arm
	// are written whenever arms are shown OR the player is holding something; the
	// left arm only when arms are shown. Skipped segments keep stale transforms,
	// which is harmless because their entities are hidden.
	if(bShowArms || mbHoldingWorldObject || bGunUp)
	{
		SetSegmentMatrix(eAvatarEntity_ShoulderBar, vShL, vShR, gfLimbThickness);

		SetSegmentMatrix(eAvatarEntity_UpperArmR, vShR, vElbowR, gfLimbThickness);
		SetSegmentMatrix(eAvatarEntity_ForeArmR, vElbowR, vHandR, gfLimbThickness);

		//A short stub straight out of the hand along the aim. Thicker than a limb so
		//it does not read as a third finger at split-screen distance.
		if(bGunUp)
		{
			SetSegmentMatrix(eAvatarEntity_Gun, vHandR,
							 vHandR + pCam->GetForward() * 0.26f, gfLimbThickness * 1.45f);
		}

		if(bShowArms)
		{
			SetSegmentMatrix(eAvatarEntity_UpperArmL, vShL, vElbowL, gfLimbThickness);
			SetSegmentMatrix(eAvatarEntity_ForeArmL, vElbowL, vHandL, gfLimbThickness);
		}
	}

	// Head + nose: at the camera position, rotating with the view (same
	// camera-follow matrix idiom as the player hands).
	cMatrixf mtxHead = cMath::MatrixRotate(
		cVector3f(pCam->GetPitch(), pCam->GetYaw(), pCam->GetRoll()),
		eEulerRotationOrder_ZXY);
	mtxHead.SetTranslation(vCamPos);

	if(mvEntities[eAvatarEntity_Head]) mvEntities[eAvatarEntity_Head]->SetMatrix(mtxHead);
	if(mvEntities[eAvatarEntity_Nose]) mvEntities[eAvatarEntity_Nose]->SetMatrix(mtxHead);
	//Built around the head's own origin, so it rides the same matrix exactly.
	if(mvEntities[eAvatarEntity_Hair]) mvEntities[eAvatarEntity_Hair]->SetMatrix(mtxHead);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cVector3f cLuxPlayerAvatar::SolveIK(const cVector3f &avRoot, cVector3f &avEnd, float afBoneLen, const cVector3f &avBendDir)
{
	cVector3f vDelta = avEnd - avRoot;
	float fDist = vDelta.Length();
	float fMaxDist = afBoneLen * 2.0f * 0.999f;

	// Clamp the end point to the reachable range.
	if(fDist > fMaxDist && fDist > 0.001f)
	{
		vDelta = vDelta * (fMaxDist / fDist);
		avEnd = avRoot + vDelta;
		fDist = fMaxDist;
	}
	if(fDist < 0.001f)
	{
		return avRoot + cVector3f(0, -afBoneLen, 0);
	}

	cVector3f vDir = vDelta / fDist;

	// Perpendicular distance of the middle joint from the root-end line.
	float fHalfDist = fDist * 0.5f;
	float fPerpSqr = afBoneLen * afBoneLen - fHalfDist * fHalfDist;
	float fPerp = fPerpSqr > 0 ? sqrtf(fPerpSqr) : 0;

	// Bend direction, orthogonalized against the root-end line.
	cVector3f vBend = avBendDir - vDir * cMath::Vector3Dot(avBendDir, vDir);
	float fBendLen = vBend.Length();
	if(fBendLen < 0.001f)
	{
		// Degenerate: pick any perpendicular.
		vBend = cMath::Vector3Cross(vDir, cVector3f(0, 1, 0));
		if(vBend.Length() < 0.001f) vBend = cMath::Vector3Cross(vDir, cVector3f(1, 0, 0));
		fBendLen = vBend.Length();
	}
	vBend = vBend / fBendLen;

	return avRoot + vDir * fHalfDist + vBend * fPerp;
}

//-----------------------------------------------------------------------

void cLuxPlayerAvatar::EnsureRigForModel(cWorld *apWorld)
{
	if(apWorld==NULL) return;

	int lWanted = mpPlayer && mpPlayer->IsPlayer2()
				? ImGuiDebugMenu::GetCoopModelP2()
				: ImGuiDebugMenu::GetCoopModelP1();

	if(lWanted < 0 || lWanted >= eAvatarModel_LastEnum) lWanted = eAvatarModel_Stickman;

	//Already showing the right thing.
	if(lWanted == mlRigModel && (lWanted == eAvatarModel_Stickman || mpRigEntity)) return;

	//Already tried, bones did not resolve. Retrying rebuilds a skinned mesh every
	//call for no possible gain.
	if(lWanted == mlFailedRigModel) return;

	DestroyRig();
	mlRigModel = lWanted;

	if(lWanted == eAvatarModel_Stickman) return;

	const cLuxAvatarRigDef &def = gvAvatarRigs[lWanted];

	cMeshManager *pMeshManager = gpBase->mpEngine->GetResources()->GetMeshManager();

	//The raw mesh, NOT the .ent. Going through cWorld::CreateEntity would hand
	//servant_grunt.ent to iLuxEnemyLoader, which only declines to build a real enemy
	//because gpBase->mpCurrentMapLoading happens to be NULL outside map load -- and
	//during a load it would instead give us a monster with AI and a character body.
	mpRigMesh = pMeshManager->CreateMesh(def.msMesh);
	if(mpRigMesh==NULL)
	{
		Error("Could not load co-op avatar mesh '%s'\n", def.msMesh);
		mlRigModel = eAvatarModel_Stickman;
		return;
	}

	//One reference for the entity we are about to make (its destructor releases one
	//through the manager), and one for us so the mesh survives between maps. Exactly
	//the arrangement the procedural meshes use.
	mpRigMesh->IncUserCount();

	tString sName = mpPlayer && mpPlayer->IsPlayer2() ? "CoopModelP2" : "CoopModelP1";
	mpRigEntity = apWorld->CreateMeshEntity(sName, mpRigMesh, false);
	if(mpRigEntity==NULL)
	{
		pMeshManager->Destroy(mpRigMesh);	//gives back the entity's reference
		pMeshManager->Destroy(mpRigMesh);	//and ours
		mpRigMesh = NULL;
		mlRigModel = eAvatarModel_Stickman;
		return;
	}

	//THE load-bearing line. cMeshEntity::UpdateLogic resets every bone to its bind
	//pose each frame while any animation state exists but none is active -- the
	//`bAnimationActive || mbUpdatedBones==false` test, where mbUpdatedBones is only
	//ever set by the skeleton-physics path. With NO animation states the whole
	//skeleton block is skipped instead, and hand-set bone matrices survive, resolve
	//through cNode3D's lazy world transform, and reach the skinning palette.
	mpRigEntity->ClearAnimations();

	mpRigEntity->SetVisible(false);
	mpRigEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, false);

	if(ResolveRigBones()==false)
	{
		Error("Co-op avatar rig '%s' is missing bones; falling back to the stickman\n", def.msName);
		DestroyRig();
		mlRigModel = eAvatarModel_Stickman;

		//Remember the request that failed, or the early-out at the top can never match
		//again -- mlRigModel says Stickman while the menu still asks for this model, so
		//every call destroyed and rebuilt a skinned mesh. Twice per player per viewport
		//is four full rebuilds a frame, forever, each churning the container.
		mlFailedRigModel = lWanted;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerAvatar::DestroyRig()
{
	if(mpRigEntity)
	{
		//Only through the world that owns it, and only while that world is still
		//registered. Anything else is the use-after-free the stickman path documents.
		if(mpEntityWorld && gpBase->mpEngine->GetScene()->WorldExists(mpEntityWorld))
			mpEntityWorld->DestroyMeshEntity(mpRigEntity);

		mpRigEntity = NULL;
	}

	if(mpRigMesh)
	{
		gpBase->mpEngine->GetResources()->GetMeshManager()->Destroy(mpRigMesh);
		mpRigMesh = NULL;
	}

	for(int i=0; i<eRigBone_LastEnum; ++i)
	{
		mvRigBones[i] = NULL;
		mvRigBind[i] = NULL;
	}
	mvRigFoot[0] = NULL;
	mvRigFoot[1] = NULL;
}

//-----------------------------------------------------------------------

bool cLuxPlayerAvatar::ResolveRigBones()
{
	if(mpRigEntity==NULL || mpRigMesh==NULL) return false;

	cSkeleton *pSkel = mpRigMesh->GetSkeleton();
	if(pSkel==NULL) return false;

	const cLuxAvatarRigDef &def = gvAvatarRigs[mlRigModel];

	//Bind bones first, walking each limb so a NULL entry means "first child".
	cBone *pBind[eRigBone_LastEnum];
	cBone *pTip[eRigBone_LastEnum];

	//The pelvis and the chest both aim along the torso, so both take the spine's
	//tip as their target. The chest is optional and NULL on four of the five rigs.
	pBind[eRigBone_Root] = def.msRoot ? pSkel->GetBoneByName(def.msRoot) : NULL;
	pTip[eRigBone_Root] = pSkel->GetBoneByName(def.msSpine);

	pBind[eRigBone_Spine] = pSkel->GetBoneByName(def.msSpine);
	pTip[eRigBone_Spine] = pSkel->GetBoneByName(def.msSpineTip);

	pBind[eRigBone_Chest] = def.msChest ? pSkel->GetBoneByName(def.msChest) : NULL;
	pTip[eRigBone_Chest] = pSkel->GetBoneByName(def.msSpineTip);

	pBind[eRigBone_Head] = pSkel->GetBoneByName(def.msHead);
	pTip[eRigBone_Head] = gAvatarFirstChild(pBind[eRigBone_Head]);

	const char *const *ppChain[4] = { def.msArmL, def.msArmR, def.msLegL, def.msLegR };
	const int lRoot[4] = { eRigBone_ShoulderL, eRigBone_ShoulderR, eRigBone_HipL, eRigBone_HipR };

	for(int c=0; c<4; ++c)
	{
		cBone *pA = pSkel->GetBoneByName(ppChain[c][0]);
		cBone *pB = gAvatarResolveBone(pSkel, pA, ppChain[c][1]);
		cBone *pC = gAvatarResolveBone(pSkel, pB, ppChain[c][2]);

		pBind[lRoot[c]]   = pA;	pTip[lRoot[c]]   = pB;
		pBind[lRoot[c]+1] = pB;	pTip[lRoot[c]+1] = pC;

		//Cleared before it is filled, because this runs again every time the model is
		//switched from the debug menu and the constructor is the only other place that
		//touches it. A chain the new rig cannot resolve would otherwise keep rolling
		//against the PREVIOUS model's bind plane.
		mvRigBindRef[lRoot[c]]   = cVector3f(0,0,0);
		mvRigBindRef[lRoot[c]+1] = cVector3f(0,0,0);

		//The limb's PLANE NORMAL in bind -- the same quantity Update measures each
		//frame, so the roll is a delta between two like things. Both bones of the
		//chain share it, because both of them lie in that plane.
		//
		//Stored rather than the in-plane offset the middle joint sits at, which was
		//the previous answer and the wrong one: that vector is perpendicular to the
		//root-to-tip line, not to either bone, so PointBone's flattening mangles it by
		//a different amount per bone and the elbow comes apart. See gAvatarLimbNormal.
		//
		//The bind bend is small on some rigs -- Alexander 3.4 degrees, Agrippa 4.5,
		//the grunt 67 -- but it is deterministic, not noise: the offset comes out -Z
		//on both arms and +Z on both legs of every rig, elbows back and knees forward,
		//which is a rigger's A-pose and not an accident. A bind limb that really is
		//dead straight carries no roll information at all and is left at zero, which
		//PointBone reads as "do not roll".
		if(pA && pB && pC)
		{
			cVector3f vRoot = pA->GetWorldTransform().GetTranslation();
			cVector3f vMid  = pB->GetWorldTransform().GetTranslation();
			cVector3f vTip  = pC->GetWorldTransform().GetTranslation();

			//The degenerate case is caught HERE rather than left to
			//gAvatarLimbNormal's fallback. That fallback exists to match where
			//SolveIK actually put a joint, which is the right answer at runtime and
			//the wrong one for a bind pose: inventing a plane for a limb that has
			//none would roll the bone by a fixed arbitrary angle for the rest of the
			//model's life. None of the five rigs is anywhere near this -- the
			//tightest is Alexander at 1.5% of limb length -- so it is a guard for the
			//sixth model, not for these.
			cVector3f vAxis = vTip - vRoot;
			cVector3f vOff = vMid - vRoot;
			if(vAxis.SqrLength() > 0.0000001f)
			{
				vAxis.Normalize();
				vOff = vOff - vAxis * cMath::Vector3Dot(vOff, vAxis);
			}

			cVector3f vNorm = vOff.SqrLength() > 0.000001f
							? gAvatarLimbNormal(vRoot, vTip, vMid - vRoot)
							: cVector3f(0,0,0);

			if(vNorm.SqrLength() > 0.0000001f)
			{
				mvRigBindRef[lRoot[c]]   = vNorm;
				mvRigBindRef[lRoot[c]+1] = vNorm;
			}
		}

		//Legs are chains 2 and 3. Measure them once, in the bind pose: rotation-only
		//posing never changes a bone's length, so these hold for good and the foot
		//height can be computed each frame instead of read off a bone state that
		//nothing updates. See the plant pass in Update.
		if(c >= 2 && pA && pB && pC)
		{
			const int l = c - 2;
			cVector3f vHip  = pA->GetWorldTransform().GetTranslation();
			cVector3f vKnee = pB->GetWorldTransform().GetTranslation();
			cVector3f vFoot = pC->GetWorldTransform().GetTranslation();

			mfRigHipY[l] = vHip.y;
			mfRigThighLen[l] = (vKnee - vHip).Length();
			mfRigShinLen[l] = (vFoot - vKnee).Length();

			//Where the ankle rests in bind. Everything the plant pass does is measured
			//from here, so ankle height, boot thickness and wherever the rig puts its
			//origin all cancel out.
			mfRigFootYBind[l] = vFoot.y;

			mvRigFoot[l] = mpRigEntity->GetBoneStateFromName(pC->GetName());
		}
	}

	//Now the live states, and the bind DIRECTION each bone points in. Storing the
	//direction rather than the orientation is what keeps this rig-agnostic: only the
	//delta from bind-direction to solved-direction is ever applied, so no axis
	//convention has to be known -- and the five rigs do not share one.
	bool bOk = true;
	for(int i=0; i<eRigBone_LastEnum; ++i)
	{
		mvRigBind[i] = pBind[i];
		mvRigBones[i] = NULL;
		mvRigBindDir[i] = cVector3f(0,1,0);

		//Head, pelvis and chest are all allowed to be absent -- the rigs disagree on
		//which of them exist, and a bone left in bind still looks like that bone. A
		//limb is not optional.
		bool bOptional = (i == eRigBone_Head || i == eRigBone_Root || i == eRigBone_Chest);

		if(pBind[i]==NULL)
		{
			if(bOptional==false) bOk = false;
			continue;
		}

		//The head ALWAYS aims parent->bone, i.e. up the neck, which is the line a
		//head bone continues by definition.
		//
		//Using bone->first-child here is what broke it twice over: four of the five
		//rigs have no bone past the head at all, so the head was skipped entirely and
		//never moved -- and Alexander DOES have one, a "hair" bone, which points
		//somewhere useless and gave a bind direction that was not up the skull. Going
		//through the parent removes both failure modes and the special case with them.
		bool bUseParentDir = (i == eRigBone_Head && pBind[i]->GetParent() != NULL);

		if(bUseParentDir==false && pTip[i]==NULL)
		{
			if(bOptional==false) bOk = false;
			continue;
		}

		mvRigBones[i] = mpRigEntity->GetBoneStateFromName(pBind[i]->GetName());
		if(mvRigBones[i]==NULL)
		{
			if(bOptional==false) bOk = false;
			continue;
		}

		cVector3f vDir;
		if(bUseParentDir)
			vDir = pBind[i]->GetWorldTransform().GetTranslation() -
				   pBind[i]->GetParent()->GetWorldTransform().GetTranslation();
		else
			vDir = pTip[i]->GetWorldTransform().GetTranslation() -
				   pBind[i]->GetWorldTransform().GetTranslation();

		if(vDir.SqrLength() < 0.0000001f)
		{
			//Zero-length bone: nothing to point. Leave it in bind pose.
			mvRigBones[i] = NULL;
			continue;
		}

		vDir.Normalize();
		mvRigBindDir[i] = vDir;
	}

	return bOk;
}

//-----------------------------------------------------------------------

void cLuxPlayerAvatar::PitchBone(int alBone, const cVector3f &avAxisWorld, float afAngle)
{
	cBoneState *pState = mvRigBones[alBone];
	cBone *pBind = mvRigBind[alBone];
	if(pState==NULL || pBind==NULL) return;

	cNode3D *pParent = pState->GetParent();
	if(pParent==NULL) return;

	cVector3f vAxis = avAxisWorld;
	if(vAxis.SqrLength() < 0.0000001f) return;
	vAxis.Normalize();

	//The axis is given in world space; the rotation has to be composed in the
	//PARENT's space, because that is the frame a bone's local matrix lives in.
	cMatrixf mtxParentRot = pParent->GetWorldMatrix();
	mtxParentRot.SetTranslation(cVector3f(0,0,0));

	cVector3f vAxisLocal = cMath::MatrixMul(cMath::MatrixInverse(mtxParentRot), vAxis);
	if(vAxisLocal.SqrLength() < 0.0000001f) return;
	vAxisLocal.Normalize();

	//Composed ONTO the bind pose, not replacing it. No arc is solved, so roll is
	//simply left alone -- which is the whole point of this function existing.
	cMatrixf mtxLocal = cMath::MatrixMul(
						cMath::MatrixQuaternion(cQuaternion(afAngle, vAxisLocal)),
						pBind->GetLocalTransform());

	//Same rule as PointBone: the rig's own offset from its parent is kept, so the
	//head pivots at the neck rather than swinging away from it.
	mtxLocal.SetTranslation(pBind->GetLocalTransform().GetTranslation());

	pState->SetMatrix(mtxLocal);
}

//-----------------------------------------------------------------------

void cLuxPlayerAvatar::PointBone(int alBone, const cVector3f &avDirWorld,
								 const cVector3f &avRefWorld)
{
	cBoneState *pState = mvRigBones[alBone];
	cBone *pBind = mvRigBind[alBone];
	if(pState==NULL || pBind==NULL || mpRigEntity==NULL) return;

	cNode3D *pParent = pState->GetParent();
	if(pParent==NULL) return;

	cVector3f vWant = avDirWorld;
	if(vWant.SqrLength() < 0.0000001f) return;
	vWant.Normalize();

	//Work in MODEL space, which is where the bind directions live.
	cMatrixf mtxEntRot = mpRigEntity->GetWorldMatrix();
	mtxEntRot.SetTranslation(cVector3f(0,0,0));

	cVector3f vWantModel = cMath::MatrixMul(cMath::MatrixInverse(mtxEntRot), vWant);
	if(vWantModel.SqrLength() < 0.0000001f) return;
	vWantModel.Normalize();

	//Only the DELTA from the bind direction is applied, so no rig's axis
	//convention has to be known -- and these five do not share one.
	cMatrixf mtxBindRot = pBind->GetWorldTransform();
	mtxBindRot.SetTranslation(cVector3f(0,0,0));

	cMatrixf mtxAim = gAvatarRotateFromTo(mvRigBindDir[alBone], vWantModel);

	//Roll, if the caller knows which plane this limb is bending in.
	//
	//The aim above fixes where the bone POINTS and says nothing about how it is
	//twisted around its own length -- a shortest arc has no opinion on that. A
	//cylinder does not care; a forearm very much does, which is what made elbows
	//look screwed on sideways while holding something.
	//
	//The reference is a limb plane normal, so it is already perpendicular to the aim
	//and the flattening below is a formality that cannot remove anything. It is there
	//for the arithmetic, not for the geometry.
	if(avRefWorld.SqrLength() > 0.0000001f && mvRigBindRef[alBone].SqrLength() > 0.0000001f)
	{
		cVector3f vRefWantModel = cMath::MatrixMul(cMath::MatrixInverse(mtxEntRot), avRefWorld);

		cVector3f vRefHave = cMath::MatrixMul(mtxAim, mvRigBindRef[alBone]);

		vRefHave     = vRefHave     - vWantModel * cMath::Vector3Dot(vRefHave, vWantModel);
		vRefWantModel = vRefWantModel - vWantModel * cMath::Vector3Dot(vRefWantModel, vWantModel);

		if(vRefHave.SqrLength() > 0.0000001f && vRefWantModel.SqrLength() > 0.0000001f)
		{
			vRefHave.Normalize();
			vRefWantModel.Normalize();

			//Taken as an ANGLE about the aim, not solved as an arc between the two.
			//
			//gAvatarRotateFromTo is a shortest arc, and for exactly-opposed vectors there
			//is no shortest arc, so it picks an arbitrary perpendicular. Correct for
			//aiming; wrong here, because this is only a ROLL while its axis is the aim,
			//and any other axis swings the bone off the direction just solved for. A limb
			//needing a half turn of roll is not a corner case -- it is the lantern pose at
			//about ten degrees of look-down, where it cost 44 degrees of forearm.
			//
			//atan2 of the sine and cosine about that axis is continuous through 180, has
			//no degenerate case, and cannot disturb the aim because the axis IS the aim.
			float fRoll = atan2f(
					cMath::Vector3Dot(cMath::Vector3Cross(vRefHave, vRefWantModel), vWantModel),
					cMath::Vector3Dot(vRefHave, vRefWantModel));

			mtxAim = cMath::MatrixMul(
						cMath::MatrixQuaternion(cQuaternion(fRoll, vWantModel)),
						mtxAim);
		}
	}

	cMatrixf mtxDesiredModel = cMath::MatrixMul(mtxAim, mtxBindRot);

	cMatrixf mtxDesiredWorld = cMath::MatrixMul(mtxEntRot, mtxDesiredModel);

	//Into the parent's space. Bone states are world-space (the state root hangs off
	//the entity) and parents are written before their children in Update, so this
	//reads a parent that is already correct.
	cMatrixf mtxParentRot = pParent->GetWorldMatrix();
	mtxParentRot.SetTranslation(cVector3f(0,0,0));

	cMatrixf mtxLocal = cMath::MatrixMul(cMath::MatrixInverse(mtxParentRot), mtxDesiredWorld);

	//THE line that keeps the model its true size: the bone's offset from its parent
	//is left exactly as the rig authored it, so limb lengths are the model's own.
	//Writing a solved POSITION here instead is what squashed and stretched it.
	mtxLocal.SetTranslation(pBind->GetLocalTransform().GetTranslation());

	pState->SetMatrix(mtxLocal);
}

//-----------------------------------------------------------------------

void cLuxPlayerAvatar::SetSegmentMatrix(eAvatarEntity aEntity, const cVector3f &avStart, const cVector3f &avEnd, float afThickness)
{
	cMeshEntity *pEnt = mvEntities[aEntity];
	if(pEnt == NULL) return;

	cVector3f vDelta = avEnd - avStart;
	float fLen = vDelta.Length();
	if(fLen < 0.002f) fLen = 0.002f;

	cVector3f vY = vDelta / fLen;
	cVector3f vRef = (fabsf(vY.y) < 0.95f) ? cVector3f(0, 1, 0) : cVector3f(1, 0, 0);
	cVector3f vX = cMath::Vector3Cross(vRef, vY);
	vX.Normalize();
	cVector3f vZ = cMath::Vector3Cross(vX, vY);

	// Bake scale into the basis: unit cylinder is diameter 1 / height 1.
	vX = vX * afThickness;
	vZ = vZ * afThickness;
	cVector3f vYs = vY * fLen;
	cVector3f vMid = (avStart + avEnd) * 0.5f;

	cMatrixf mtxSeg(vX.x, vYs.x, vZ.x, vMid.x,
					vX.y, vYs.y, vZ.y, vMid.y,
					vX.z, vYs.z, vZ.z, vMid.z,
					0, 0, 0, 1);

	pEnt->SetMatrix(mtxSeg);
}

//-----------------------------------------------------------------------

cMesh* cLuxPlayerAvatar::CreateMesh(const tString &asName, iVertexBuffer *apVB, cMaterial *apMaterial)
{
	cResources *pResources = gpBase->mpEngine->GetResources();

	cMesh *pMesh = hplNew( cMesh, (asName, _W(""), pResources->GetMaterialManager(), pResources->GetAnimationManager()) );

	cSubMesh *pSubMesh = pMesh->CreateSubMesh("Main");
	pSubMesh->SetMaterial(apMaterial ? apMaterial : mpMaterial);
	pSubMesh->SetVertexBuffer(apVB);

	return pMesh;
}

//-----------------------------------------------------------------------

iVertexBuffer* cLuxPlayerAvatar::CreateVertexBufferBase(int alVtxNum, int alIdxNum)
{
	iLowLevelGraphics *pLowGfx = gpBase->mpEngine->GetGraphics()->GetLowLevel();

	iVertexBuffer *pVB = pLowGfx->CreateVertexBuffer(
		eVertexBufferType_Hardware,
		eVertexBufferDrawType_Tri, eVertexBufferUsageType_Static,
		alVtxNum, alIdxNum);

	pVB->CreateElementArray(eVertexBufferElement_Position, eVertexBufferElementFormat_Float, 4);
	pVB->CreateElementArray(eVertexBufferElement_Normal, eVertexBufferElementFormat_Float, 3);
	pVB->CreateElementArray(eVertexBufferElement_Color0, eVertexBufferElementFormat_Float, 4);
	pVB->CreateElementArray(eVertexBufferElement_Texture0, eVertexBufferElementFormat_Float, 3);

	return pVB;
}

//-----------------------------------------------------------------------

void cLuxPlayerAvatar::AddVertex(iVertexBuffer *apVB, const cVector3f &avPos, const cVector3f &avNormal, const cVector2f &avUV)
{
	apVB->AddVertexVec3f(eVertexBufferElement_Position, avPos);
	apVB->AddVertexVec3f(eVertexBufferElement_Normal, avNormal);
	apVB->AddVertexColor(eVertexBufferElement_Color0, cColor(1, 1));
	apVB->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(avUV.x, avUV.y, 0));
}

//-----------------------------------------------------------------------

iVertexBuffer* cLuxPlayerAvatar::CreateCylinderVB(int alSegments)
{
	// Unit cylinder: radius 0.5 in XZ, height 1 along Y, centered on origin.
	int lSideVtx = (alSegments + 1) * 2;
	int lCapVtx = (alSegments + 1) * 2;
	int lIdxNum = alSegments * 6 + alSegments * 3 * 2 * 2;	// caps are double-sided

	iVertexBuffer *pVB = CreateVertexBufferBase(lSideVtx + lCapVtx, lIdxNum);

	//////////////////////////
	// Side ring vertices: [2i] = bottom, [2i+1] = top. The seam vertex is
	// duplicated (i = alSegments) so the UVs wrap cleanly.
	for(int i=0; i<=alSegments; ++i)
	{
		float fAngle = k2Pif * ((float)i / (float)alSegments);
		float fX = cosf(fAngle) * 0.5f;
		float fZ = sinf(fAngle) * 0.5f;
		cVector3f vNormal(cosf(fAngle), 0, sinf(fAngle));
		float fU = (float)i / (float)alSegments;

		AddVertex(pVB, cVector3f(fX, -0.5f, fZ), vNormal, cVector2f(fU, 0));
		AddVertex(pVB, cVector3f(fX, 0.5f, fZ), vNormal, cVector2f(fU, 1));
	}

	for(int i=0; i<alSegments; ++i)
	{
		int b0 = i*2,       t0 = i*2 + 1;
		int b1 = (i+1)*2,   t1 = (i+1)*2 + 1;

		pVB->AddIndex(b0); pVB->AddIndex(b1); pVB->AddIndex(t1);
		pVB->AddIndex(b0); pVB->AddIndex(t1); pVB->AddIndex(t0);
	}

	//////////////////////////
	// Caps: center + ring (own vertices so the caps get flat normals)
	int lCapStart = lSideVtx;
	for(int lCap=0; lCap<2; ++lCap)
	{
		float fY = (lCap == 0) ? -0.5f : 0.5f;
		cVector3f vNormal(0, (lCap == 0) ? -1.0f : 1.0f, 0);

		int lCenter = lCapStart;
		AddVertex(pVB, cVector3f(0, fY, 0), vNormal, cVector2f(0.5f, 0.5f));

		for(int i=0; i<alSegments; ++i)
		{
			float fAngle = k2Pif * ((float)i / (float)alSegments);
			AddVertex(pVB, cVector3f(cosf(fAngle)*0.5f, fY, sinf(fAngle)*0.5f), vNormal,
					  cVector2f(0.5f + cosf(fAngle)*0.5f, 0.5f + sinf(fAngle)*0.5f));
		}

		for(int i=0; i<alSegments; ++i)
		{
			int lNext = (i + 1) % alSegments;

			// Both windings, so the caps are solid whichever way the engine
			// culls — otherwise the limb ends read as open pipes.
			pVB->AddIndex(lCenter);
			pVB->AddIndex(lCenter + 1 + i);
			pVB->AddIndex(lCenter + 1 + lNext);

			pVB->AddIndex(lCenter);
			pVB->AddIndex(lCenter + 1 + lNext);
			pVB->AddIndex(lCenter + 1 + i);
		}

		lCapStart += alSegments + 1;
	}

	pVB->Compile(eVertexCompileFlag_CreateTangents);
	return pVB;
}

//-----------------------------------------------------------------------

iVertexBuffer* cLuxPlayerAvatar::CreateSphereVB(int alSlices, int alStacks, float afRadius)
{
	int lVtxNum = (alSlices + 1) * (alStacks + 1);
	int lIdxNum = alSlices * alStacks * 6;

	iVertexBuffer *pVB = CreateVertexBufferBase(lVtxNum, lIdxNum);

	for(int lStack=0; lStack <= alStacks; ++lStack)
	{
		float fPhi = kPif * ((float)lStack / (float)alStacks) - kPif*0.5f; // -90..+90
		float fY = sinf(fPhi);
		float fRingR = cosf(fPhi);

		for(int lSlice=0; lSlice <= alSlices; ++lSlice)
		{
			float fTheta = k2Pif * ((float)lSlice / (float)alSlices);
			cVector3f vNormal(fRingR * cosf(fTheta), fY, fRingR * sinf(fTheta));

			AddVertex(pVB, vNormal * afRadius, vNormal,
					  cVector2f((float)lSlice / (float)alSlices, (float)lStack / (float)alStacks));
		}
	}

	for(int lStack=0; lStack < alStacks; ++lStack)
	{
		for(int lSlice=0; lSlice < alSlices; ++lSlice)
		{
			int l0 = lStack * (alSlices + 1) + lSlice;
			int l1 = l0 + 1;
			int l2 = l0 + (alSlices + 1);
			int l3 = l2 + 1;

			// Wind so the OUTSIDE faces are front-facing under the engine's
			// cull direction — the previous winding rendered the sphere
			// inside-out (near faces culled, far interior visible).
			pVB->AddIndex(l0); pVB->AddIndex(l3); pVB->AddIndex(l2);
			pVB->AddIndex(l0); pVB->AddIndex(l1); pVB->AddIndex(l3);
		}
	}

	pVB->Compile(eVertexCompileFlag_CreateTangents);
	return pVB;
}

//-----------------------------------------------------------------------

iVertexBuffer* cLuxPlayerAvatar::CreateHairVB(int alSlices, int alStacks, float afHeadRadius)
{
	const float fR = afHeadRadius * gfAvatarHairScale;

	int lVtxNum = (alSlices + 1) * (alStacks + 1);
	//Double-sided: every quad emits its two triangles twice, once per winding,
	//so the shell never disappears whichever side of it the camera is on.
	int lIdxNum = alSlices * alStacks * 6 * 2;

	iVertexBuffer *pVB = CreateVertexBufferBase(lVtxNum, lIdxNum);

	for(int lStack=0; lStack <= alStacks; ++lStack)
	{
		float fT = (float)lStack / (float)alStacks;	// 0 = crown, 1 = the ends

		for(int lSlice=0; lSlice <= alSlices; ++lSlice)
		{
			float fTheta = k2Pif * ((float)lSlice / (float)alSlices);
			float fCosT = cosf(fTheta);
			float fSinT = sinf(fTheta);

			//Same axes as CreateSphereVB: x = cos, z = sin. The nose points -Z, so
			//+Z is the back of the skull and this runs 0 at the face to 1 behind.
			float fBackness = (fSinT + 1.0f) * 0.5f;
			float fDrop = gfAvatarHairFrontDrop +
							(gfAvatarHairBackDrop - gfAvatarHairFrontDrop) * fBackness;

			//Per-azimuth polar limit -- this one lerp is the whole difference
			//between a wig and a helmet.
			float fPolar = (kPif * 0.5f) * fDrop * fT;

			float fY = cosf(fPolar);

			//Past the equator, stop following the sphere back inwards and fall
			//straight instead. Curving in gives a bowl cut; this gives length.
			float fRing = fPolar > kPif*0.5f ? 1.0f : sinf(fPolar);

			cVector3f vPos(fRing * fCosT * fR, fY * fR, fRing * fSinT * fR);

			//Outward off the scalp on the cap, horizontal down the fall.
			cVector3f vNormal(fRing * fCosT, fY > 0 ? fY : 0.0f, fRing * fSinT);
			vNormal.Normalize();

			AddVertex(pVB, vPos, vNormal,
					  cVector2f((float)lSlice / (float)alSlices, fT));
		}
	}

	for(int lStack=0; lStack < alStacks; ++lStack)
	{
		for(int lSlice=0; lSlice < alSlices; ++lSlice)
		{
			int l0 = lStack * (alSlices + 1) + lSlice;
			int l1 = l0 + 1;
			int l2 = l0 + (alSlices + 1);
			int l3 = l2 + 1;

			//Outside. REVERSED from CreateSphereVB rather than copied from it: that
			//function walks its stacks bottom-to-top (phi runs -90 to +90) while this
			//one walks them crown-to-ends, so the same index order would describe the
			//opposite face.
			pVB->AddIndex(l0); pVB->AddIndex(l2); pVB->AddIndex(l3);
			pVB->AddIndex(l0); pVB->AddIndex(l3); pVB->AddIndex(l1);

			//Inside. A wig is an open shell -- the fringe, the ends and the whole
			//underside are edges you can see past, and a single-sided shell just is
			//not there from behind. Same triangles wound the other way. They keep the
			//outward normals, so the inner surface shades like the outer one rather
			//than going flat black.
			pVB->AddIndex(l0); pVB->AddIndex(l3); pVB->AddIndex(l2);
			pVB->AddIndex(l0); pVB->AddIndex(l1); pVB->AddIndex(l3);
		}
	}

	pVB->Compile(eVertexCompileFlag_CreateTangents);
	return pVB;
}

//-----------------------------------------------------------------------

iVertexBuffer* cLuxPlayerAvatar::CreateConeVB(int alSegments, float afRadius, float afLength, float afZOffset)
{
	// Cone pointing towards local -Z (the view direction): base ring in the
	// XY plane at z = afZOffset, apex at z = afZOffset - afLength.
	int lVtxNum = 1 + alSegments + 1 + alSegments;	// apex + side ring + base center + base ring
	int lIdxNum = alSegments * 3 + alSegments * 3 * 2;	// base is double-sided

	iVertexBuffer *pVB = CreateVertexBufferBase(lVtxNum, lIdxNum);

	cVector3f vApex(0, 0, afZOffset - afLength);

	// Apex
	AddVertex(pVB, vApex, cVector3f(0, 0, -1), cVector2f(0.5f, 1));

	// Side ring
	for(int i=0; i<alSegments; ++i)
	{
		float fAngle = k2Pif * ((float)i / (float)alSegments);
		float fX = cosf(fAngle);
		float fY = sinf(fAngle);

		cVector3f vNormal(fX, fY, -afRadius / afLength);
		vNormal.Normalize();

		AddVertex(pVB, cVector3f(fX*afRadius, fY*afRadius, afZOffset), vNormal,
				  cVector2f((float)i / (float)alSegments, 0));
	}

	// Base center + base ring (flat normals pointing backwards)
	int lBaseCenter = 1 + alSegments;
	AddVertex(pVB, cVector3f(0, 0, afZOffset), cVector3f(0, 0, 1), cVector2f(0.5f, 0.5f));

	for(int i=0; i<alSegments; ++i)
	{
		float fAngle = k2Pif * ((float)i / (float)alSegments);
		AddVertex(pVB, cVector3f(cosf(fAngle)*afRadius, sinf(fAngle)*afRadius, afZOffset), cVector3f(0, 0, 1),
				  cVector2f(0.5f + cosf(fAngle)*0.5f, 0.5f + sinf(fAngle)*0.5f));
	}

	// Side triangles: apex + ring
	for(int i=0; i<alSegments; ++i)
	{
		int lNext = (i + 1) % alSegments;
		pVB->AddIndex(0);
		pVB->AddIndex(1 + i);
		pVB->AddIndex(1 + lNext);
	}
	// Base fan (double-sided so the nose is solid whichever way culling goes)
	for(int i=0; i<alSegments; ++i)
	{
		int lNext = (i + 1) % alSegments;
		pVB->AddIndex(lBaseCenter);
		pVB->AddIndex(lBaseCenter + 1 + i);
		pVB->AddIndex(lBaseCenter + 1 + lNext);

		pVB->AddIndex(lBaseCenter);
		pVB->AddIndex(lBaseCenter + 1 + lNext);
		pVB->AddIndex(lBaseCenter + 1 + i);
	}

	pVB->Compile(eVertexCompileFlag_CreateTangents);
	return pVB;
}

//-----------------------------------------------------------------------
