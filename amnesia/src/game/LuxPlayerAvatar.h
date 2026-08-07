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

#ifndef LUX_PLAYER_AVATAR_H
#define LUX_PLAYER_AVATAR_H

//----------------------------------------------

#include "LuxBase.h"

class cLuxPlayer;
class cLuxMap;

//----------------------------------------------

/**
 * Co-op player avatar: a stickman made of REAL world mesh entities with a
 * proper soliddiffuse material (P1 = blue, P2 = red), so the deferred
 * renderer treats it like any prop: lit by lights, dark in darkness, visible
 * in water reflections, and consistent with bloom/fog/post effects.
 *
 *  - Head: sphere at the player's camera position, rotating with the view,
 *    with a cone "nose" showing the look direction.
 *  - Body: torso, hip bar, and two 2-bone legs drawn as thin capsule
 *    segments. Shoulders + 2-bone arms exist too but are OFF by default —
 *    the legs and arms groups are toggled live from the ImGui debug menu
 *    (turn both off for the plain floating stick + head).
 *
 * The pose is fully procedural (no animation assets): a gait phase driven by
 * the character body's real velocity swings the feet along the MOVEMENT
 * direction (so strafing/backpedaling animate correctly), stride length and
 * step rate scale with speed (walk/run emerge naturally), knees are placed
 * with 2-bone IK, and crouching follows the live collider height.
 *
 * Which viewport sees which avatar is decided per render pass by the map
 * handler's viewport callbacks (a player never sees their own avatar); see
 * SetEntitiesVisible.
 */
//----------------------------------------------

/**
 * Which body a co-op player is drawn as.
 *
 * 0 is the procedural stickman and stays the default. The rest are real Amnesia
 * character rigs, posed by the SAME solver -- their own animations are not used.
 */
enum eAvatarModel
{
	eAvatarModel_Stickman = 0,
	eAvatarModel_Grunt,
	eAvatarModel_Brute,
	eAvatarModel_Agrippa,
	eAvatarModel_Alexander,
	eAvatarModel_RitualPrisoner,
	eAvatarModel_LastEnum
};

/** The joints the solver actually drives. Everything else keeps its bind pose. */
enum eRigBone
{
	eRigBone_Root = 0,	// pelvis; left undriven it keeps whatever tilt the bind has
	eRigBone_Spine,
	eRigBone_Chest,		// optional; only the ritual prisoner's rig has one
	eRigBone_Head,
	eRigBone_ShoulderL, eRigBone_ElbowL,
	eRigBone_ShoulderR, eRigBone_ElbowR,
	eRigBone_HipL, eRigBone_KneeL,
	eRigBone_HipR, eRigBone_KneeR,
	eRigBone_LastEnum
};

//----------------------------------------------

class cLuxPlayerAvatar
{
public:
	cLuxPlayerAvatar(cLuxPlayer *apPlayer, const cColor &aColor);
	~cLuxPlayerAvatar();

	void CreateWorldEntities(cLuxMap *apMap);
	void DestroyWorldEntities(cLuxMap *apMap);

	void Update(float afTimeStep);

	// Called from the viewport pre-draw callbacks right before each viewport
	// renders, so visibility is correct per view (and thus also in that
	// view's reflections and shadow maps).
	void SetEntitiesVisible(bool abX);

	/**
	 * Make sure this avatar has parts built for the world being rendered NOW.
	 *
	 * Every part is created hidden and only ever shown by SetEntitiesVisible, so a
	 * single missed CreateWorldEntities leaves the player invisible for the whole
	 * map with nothing able to recover it. This gives that state a way back.
	 *
	 * Does nothing while a map is loading -- there is no world to attach to, and
	 * staying invisible until there is one is correct.
	 */
	void EnsureEntitiesForCurrentWorld();

	const cColor& GetColor(){ return mColor; }

private:
	enum eAvatarEntity
	{
		eAvatarEntity_Torso = 0,
		eAvatarEntity_ShoulderBar,
		eAvatarEntity_HipBar,
		eAvatarEntity_ThighL, eAvatarEntity_ShinL,
		eAvatarEntity_ThighR, eAvatarEntity_ShinR,
		eAvatarEntity_UpperArmL, eAvatarEntity_ForeArmL,
		eAvatarEntity_UpperArmR, eAvatarEntity_ForeArmR,
		eAvatarEntity_Head,
		eAvatarEntity_Nose,
		eAvatarEntity_Hair,
		eAvatarEntity_Gun,
		eAvatarEntity_LastEnum
	};

	// Is this body part currently enabled by the debug menu toggles?
	// (Legs and arms groups can be switched off; torso/head/nose always show.)
	bool PartEnabled(eAvatarEntity aEntity);

	/** Is this player's debug gun raised? Poses and shows the right arm. */
	bool GunIsUp();

	/**
	 * Drop entity pointers whose world is already gone, releasing the mesh
	 * reference CreateWorldEntities() took for each. Never dereferences the
	 * entities themselves -- they are already freed.
	 */
	void AbandonStaleEntities();

	iVertexBuffer* CreateVertexBufferBase(int alVtxNum, int alIdxNum);
	void AddVertex(iVertexBuffer *apVB, const cVector3f &avPos, const cVector3f &avNormal, const cVector2f &avUV);

	iVertexBuffer* CreateCylinderVB(int alSegments);
	iVertexBuffer* CreateSphereVB(int alSlices, int alStacks, float afRadius);
	/**
	 * A wig: a shell just clear of the head sphere whose length varies with
	 * azimuth -- a fringe at the face (-Z, where the nose points) and length
	 * down the back (+Z). Below the equator it falls straight rather than
	 * curving back in, which is what separates hair from a bowl cut.
	 */
	iVertexBuffer* CreateHairVB(int alSlices, int alStacks, float afHeadRadius);
	iVertexBuffer* CreateConeVB(int alSegments, float afRadius, float afLength, float afZOffset);

	cMesh* CreateMesh(const tString &asName, iVertexBuffer *apVB, cMaterial *apMaterial = NULL);

	void SetSegmentMatrix(eAvatarEntity aEntity, const cVector3f &avStart, const cVector3f &avEnd, float afThickness);

	//////////////////////
	// Rigged model

	/** Build or tear down the rig so it matches the debug menu. Cheap when already right. */
	void EnsureRigForModel(cWorld *apWorld);
	void DestroyRig();
	bool ResolveRigBones();

	/**
	 * Turn one bone to point along a world direction. ROTATION ONLY.
	 *
	 * The bone keeps the local translation the rig authored, so the model stays its
	 * own true size with its own limb lengths. Driving bone POSITIONS instead --
	 * which is what the stickman does, and what this used to do -- rescales every
	 * bone to the player's proportions and squashes the model.
	 */
	/**
	 * Aim a bone along a world direction, optionally pinning its roll.
	 *
	 * avRefWorld is the limb's bend-plane normal. Pass it for anything with a real
	 * twist -- arms, legs -- and the bone lands with the same roll relative to that
	 * plane as it had in bind. Pass zero and only the aim is set, which is all a
	 * shortest arc can do and all a cylinder ever needed.
	 */
	void PointBone(int alBone, const cVector3f &avDirWorld,
				   const cVector3f &avRefWorld = cVector3f(0,0,0));

	/**
	 * Rotate one bone by an explicit angle about an explicit world axis, on top of
	 * its bind pose. ROTATION ONLY, like PointBone.
	 *
	 * For anything where ROLL matters -- a head -- this is the one to use. PointBone
	 * solves a shortest arc, and a shortest arc has no opinion about roll: it is
	 * whatever falls out of the axis. On a head that is the difference between
	 * looking up and being upside down. Here nothing touches roll, so nothing can
	 * get it wrong, and there is no arc to go degenerate.
	 */
	void PitchBone(int alBone, const cVector3f &avAxisWorld, float afAngle);

	/** True when a character model is selected for this player, and it loaded. */
	bool RigActive(){ return mpRigEntity != NULL;}

	/**
	 * 2-bone IK: returns the middle joint (knee/elbow) between avRoot and
	 * avEnd for equal-length bones, bending towards avBendDir. Clamps avEnd
	 * to the reachable range.
	 */
	cVector3f SolveIK(const cVector3f &avRoot, cVector3f &avEnd, float afBoneLen, const cVector3f &avBendDir);

	cLuxPlayer *mpPlayer;
	cColor mColor;

	iTexture *mpColorTexture;
	cMaterial *mpMaterial;

	// The wig gets its own near-black material rather than the player colour.
	iTexture *mpHairTexture;
	cMaterial *mpHairMaterial;

	cMesh *mpLimbMesh;	// unit cylinder (diameter 1, height 1, centered), scaled per segment
	cMesh *mpHeadMesh;	// sphere at real size, centered on origin
	cMesh *mpNoseMesh;	// cone at real size, pointing towards local -Z (view direction)
	cMesh *mpHairMesh;	// wig shell, sits on the head and follows it exactly
	cMesh *mpGunMesh;	// black stub in the right hand when the debug gun is up

	//////////////////////
	// Rigged character model (optional; stickman is model 0)

	// Which rig this player's entity was BUILT for. Compared against the debug
	// menu every frame so switching model rebuilds rather than mis-poses.
	int mlRigModel;
	int mlFailedRigModel;	//bones would not resolve; never retried

	cMesh *mpRigMesh;
	cMeshEntity *mpRigEntity;

	// Live bone states, and the same bones in the BIND pose. Both indexed by
	// eRigBone. The bind bones are what let us apply only the DELTA from bind to
	// solved, so none of this has to know each rig's axis convention -- which is
	// just as well, because the five rigs do not agree on one.
	cBoneState *mvRigBones[eRigBone_LastEnum];
	cBone *mvRigBind[eRigBone_LastEnum];
	cVector3f mvRigBindDir[eRigBone_LastEnum];

	// The bend-plane normal of each two-bone limb, in the bind pose. This is what
	// pins a limb's ROLL -- the one thing a shortest-arc aim cannot determine. Zero
	// for bones that are not part of a limb, or whose bind limb is dead straight and
	// so has no bend plane to speak of.
	cVector3f mvRigBindRef[eRigBone_LastEnum];

	// The two foot bones, kept so the model can be planted on the floor after
	// posing. Not driven -- only measured. See the foot-plant note in Update.
	cBoneState *mvRigFoot[2];

	// Bind-pose leg measurements, taken once at load: how high each hip sits above
	// the model origin, and how long the thigh and shin are. Rotation-only posing
	// never changes a bone's length, so these stay true for the life of the rig and
	// let the foot height be computed rather than measured. See the plant pass in
	// Update for why measuring it off the bone states cannot work here.
	float mfRigHipY[2];
	float mfRigThighLen[2];
	float mfRigShinLen[2];

	// The foot bone's height in the BIND pose. The plant pass shifts by how far the
	// foot has moved FROM here, never towards zero -- the foot bone is the ankle and
	// sits well above the sole, so driving it to the floor buries the model by the
	// height of its own foot.
	float mfRigFootYBind[2];

	cMeshEntity *mvEntities[eAvatarEntity_LastEnum];
	cWorld *mpEntityWorld;  // world the entities live in; used to detect stale ptrs after a map reload
	bool mbEntitiesVisible;

	// True while the player is grabbing/manipulating a world object (chair, lever,
	// door, wheel...). Latched once per Update so the arm POSE and the arm
	// VISIBILITY can never disagree — SetEntitiesVisible runs per viewport render,
	// which is more often than Update.
	bool mbHoldingWorldObject;

	// Gait state
	float mfGaitPhase;		// stepping phase (radians); L foot = sin(phase), R foot = sin(phase+pi)
	float mfSpeed;			// smoothed horizontal speed
	cVector3f mvMoveDir;	// smoothed horizontal movement direction (world space)
};

//----------------------------------------------

#endif // LUX_PLAYER_AVATAR_H
