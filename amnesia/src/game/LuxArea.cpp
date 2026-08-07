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

#include "LuxArea.h"

#include "LuxMap.h"

#include "impl/ImGuiDebugMenu.h"

//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void iLuxAreaLoader::Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform,cWorld *apWorld)
{
	cLuxMap *pMap = gpBase->mpCurrentMapLoading;
	if(pMap==NULL) return;

	iLuxArea *pArea = CreateArea(asName, alID,pMap);

	//////////////////////////////
	// Create and set body
	iPhysicsWorld *pPhysicsWorld = apWorld->GetPhysicsWorld();
	iCollideShape* pShape = pPhysicsWorld->CreateBoxShape(avSize, NULL);
	iPhysicsBody* pBody = pPhysicsWorld->CreateBody(asName,pShape);

	pBody->SetCollide(false);
	pBody->SetCollideCharacter(false);
	pBody->SetMatrix(a_mtxTransform);
	pBody->SetUserData(pArea);
	
	pArea->mpBody = pBody;

	//////////////////////////////
	// Load base properties
	pArea->mvSize = avSize;
	pArea->m_mtxTransform = a_mtxTransform;

	//////////////////
	//Load variables
	LoadVariables(pArea, apWorld);

	//////////////////////////////
	// Load type specific properties
	SetupArea(pArea, apWorld);

	pMap->AddEntity(pArea);

	pArea->SetActive(abActive);

	pArea->SetupAfterLoad(apWorld);
}

//-----------------------------------------------------------------------


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iLuxArea::iLuxArea(const tString &asName, int alID, cLuxMap *apMap, eLuxAreaType aAreaType)  : iLuxEntity(asName,alID,apMap, eLuxEntityType_Area)
{
	mAreaType = aAreaType;

	mpBody = NULL;
}

//-----------------------------------------------------------------------

iLuxArea::~iLuxArea()
{
	if(mpBody)
	{
		mpMap->GetPhysicsWorld()->DestroyBody(mpBody);
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void iLuxArea::OnRenderSolid(cRendererCallbackFunctions* apFunctions)
{
	//////////////////////////////////////////////////////////////////
	// Debug view: the same thing the level editor shows -- a wire box where an
	// otherwise invisible volume is, coloured by what kind it is.
	//
	// Drawn from the body's own transform rather than its world bounding volume
	// so a rotated area is a rotated box, which is how the editor draws it and
	// the only way the box tells you the truth about what it covers.
	if(::ImGuiDebugMenu::GetDebugView()==false) return;
	if(mpBody==NULL) return;

	//Colours picked to be told apart at a glance, warm for the ones that run
	//script and cool for the ones that are just geometry.
	cColor col(1,1,1,1);
	switch(mAreaType)
	{
	case eLuxAreaType_Script:		col = cColor(1.0f, 0.35f, 0.35f, 1); break;
	case eLuxAreaType_Flashback:	col = cColor(1.0f, 0.85f, 0.25f, 1); break;
	case eLuxAreaType_Insanity:		col = cColor(0.85f, 0.35f, 1.0f, 1); break;
	case eLuxAreaType_Examine:		col = cColor(0.35f, 1.0f, 0.55f, 1); break;
	case eLuxAreaType_Sign:			col = cColor(0.45f, 0.85f, 1.0f, 1); break;
	case eLuxAreaType_Ladder:		col = cColor(0.55f, 0.55f, 1.0f, 1); break;
	case eLuxAreaType_Liquid:		col = cColor(0.25f, 0.65f, 1.0f, 1); break;
	case eLuxAreaType_Sticky:		col = cColor(1.0f, 0.65f, 0.25f, 1); break;
	case eLuxAreaType_SlimeDamage:	col = cColor(0.65f, 1.0f, 0.25f, 1); break;
	default:						col = cColor(0.8f, 0.8f, 0.8f, 1); break;
	}

	//////////////////////////////////////////////////////////////////
	// Inactive areas get a different LINE STYLE, not just a different shade.
	//
	// A trigger that is present but switched off looks identical to one that is
	// armed, and that is the confusion this view exists to clear up. Colour
	// alone will not carry it: dimmer reads as "further away", and the type
	// colours already use the whole palette, so a dim red still has to be told
	// apart from a bright red one behind it. A broken outline reads as "not
	// really there" at any distance and in any colour -- the same reason
	// editors have always drawn disabled geometry dashed.
	const bool bActive = IsActive();

	if(bActive==false)
	{
		col.r *= 0.55f;
		col.g *= 0.55f;
		col.b *= 0.55f;
	}

	//Corners transformed to world space and joined by hand, rather than pushing
	//a matrix and drawing a unit box. The engine's other debug draws -- the
	//pathfinder nodes, the physics shapes -- all hand DrawLine world-space
	//coordinates with no matrix in force, and that immediate-mode path does not
	//honour the renderer's matrix state. Doing it this way keeps the box
	//ORIENTED, which an axis-aligned bounding box would not, and a rotated
	//trigger drawn as an upright box is a lie about what it covers.
	iLowLevelGraphics *pGfx = apFunctions->GetLowLevelGfx();

	const cVector3f vHalf = mvSize * 0.5f;

	cVector3f vCorner[8];
	for(int i=0; i<8; ++i)
	{
		const cVector3f vLocal(	(i & 1) ? vHalf.x : -vHalf.x,
								(i & 2) ? vHalf.y : -vHalf.y,
								(i & 4) ? vHalf.z : -vHalf.z);

		vCorner[i] = cMath::MatrixMul(m_mtxTransform, vLocal);
	}

	//Pairs that differ in exactly one bit are the twelve edges of the box.
	//
	//Active draws each edge whole. Inactive chops it into dashes -- an odd
	//number of pieces with every other one drawn, so both ends of every edge
	//are always drawn and the corners still meet. A dashed box with missing
	//corners just looks like a broken box.
	const int lDashes = bActive ? 1 : 7;

	for(int i=0; i<8; ++i)
	{
		for(int lBit=1; lBit<=4; lBit<<=1)
		{
			const int j = i | lBit;
			if(j == i) continue;

			for(int d=0; d<lDashes; d+=2)
			{
				const float fFrom = (float)d / (float)lDashes;
				const float fTo   = (float)(d+1) / (float)lDashes;

				pGfx->DrawLine(	vCorner[i] + (vCorner[j] - vCorner[i]) * fFrom,
								vCorner[i] + (vCorner[j] - vCorner[i]) * fTo,
								col);
			}
		}
	}
}

//-----------------------------------------------------------------------

bool iLuxArea::CanInteract(iPhysicsBody *apBody)
{
	return false;
}

//-----------------------------------------------------------------------

bool iLuxArea::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	return false;
}

//-----------------------------------------------------------------------

eLuxFocusCrosshair iLuxArea::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	return eLuxFocusCrosshair_LastEnum;
}

//-----------------------------------------------------------------------

iEntity3D* iLuxArea::GetAttachEntity()
{
	return mpBody;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

void iLuxArea::OnSetActive(bool abX)
{	
	///////////////
	//Bodies
	if(mpBody)
		mpBody->SetActive(abX);
}

//-----------------------------------------------------------------------

void iLuxArea::OnUpdate(float afTimeStep)
{
	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerializeVirtual(iLuxArea_SaveData, iLuxEntity_SaveData)
kSerializeVar(mvSize,eSerializeType_Vector3f)
kSerializeVar(m_mtxTransform,eSerializeType_Matrixf)
kEndSerialize()


//-----------------------------------------------------------------------

iLuxEntity* iLuxArea_SaveData::CreateEntity(cLuxMap *apMap)
{
	iLuxArea *pArea = CreateArea(apMap);
	apMap->AddEntity(pArea);

	return pArea;
}

//-----------------------------------------------------------------------

void iLuxArea::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::SaveToSaveData(apSaveData);
	iLuxArea_SaveData *pData = static_cast<iLuxArea_SaveData*>(apSaveData);

    kCopyToVar(pData, mvSize);
	kCopyToVar(pData, m_mtxTransform);
}

//-----------------------------------------------------------------------

void iLuxArea::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::LoadFromSaveData(apSaveData);
	iLuxArea_SaveData *pData = static_cast<iLuxArea_SaveData*>(apSaveData);

	kCopyFromVar(pData, mvSize);
	kCopyFromVar(pData, m_mtxTransform);

	//////////////////////////////
	// Create and set body
	iPhysicsWorld *pPhysicsWorld = mpMap->GetPhysicsWorld();
	iCollideShape* pShape = pPhysicsWorld->CreateBoxShape(mvSize, NULL);
	iPhysicsBody* pBody = pPhysicsWorld->CreateBody(msName,pShape);

	pBody->SetCollide(false);
	pBody->SetCollideCharacter(false);
	pBody->SetMatrix(m_mtxTransform);
	pBody->SetUserData(this);
	pBody->SetActive(mbActive);

	mpBody = pBody;
	
	///////////////////
	//Do setup
	SetupAfterLoad(mpMap->GetWorld());
}

//-----------------------------------------------------------------------

void iLuxArea::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);
	iLuxArea_SaveData *pData = static_cast<iLuxArea_SaveData*>(apSaveData);
}

//-----------------------------------------------------------------------

