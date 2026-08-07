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

#include "LuxPlayerState_UseItem.h"

#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"
#include "LuxInventory.h"
#include "LuxMapHandler.h"
#include "LuxMapHelper.h"
#include "LuxMap.h"
#include "LuxEntity.h"
#include "LuxProp.h"
#include "LuxProp_Lamp.h"
#include "LuxMessageHandler.h"
#include "LuxHelpFuncs.h"
#include "impl/ImGuiDebugMenu.h"



//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerState_UseItem::cLuxPlayerState_UseItem(cLuxPlayer *apPlayer) : iLuxPlayerState_DefaultBase(apPlayer, eLuxPlayerState_UseItem)
{
	mpCurrentItem = NULL;
	mFlashOscill.SetUp(0,1,0,1.5f,1.5f);

	mbOtherPlayerInFocus = false;

	mfMinUseItemDistance = gpBase->mpGameCfg->GetFloat("Player_Interaction", "MinUseItemDistance",0);
}

//-----------------------------------------------------------------------

cLuxPlayerState_UseItem::~cLuxPlayerState_UseItem()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerState_UseItem::ImplementedOnEnterState(eLuxPlayerState aPrevState)
{
	mpCurrentItem = cLuxPlayerStateVars::mpInventoryItem;
	cLuxPlayerStateVars::mpInventoryItem = NULL;

	mbOtherPlayerInFocus = false;
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_UseItem::ImplementedDoAction(eLuxPlayerAction aAction,bool abPressed)
{
	////////////////////
	// Holster (draw hand object)
	if(aAction == eLuxPlayerAction_Holster && abPressed)
	{
		mpPlayer->ChangeState(eLuxPlayerState_HandObject);
		return false;
	}
	////////////////////////////
	// Interact
	if(aAction == eLuxPlayerAction_Interact || aAction == eLuxPlayerAction_Attack)
	{
		// Pressed
		if(abPressed)
		{
			// Coop: hand the item to the other player when they are in focus.
			if(mbOtherPlayerInFocus)
				GiveItemToOtherPlayer();
			else
				UseItem();

			mpPlayer->ChangeState(eLuxPlayerState_Normal);
            return false;
		}
	}

	
	return true;
}

//-----------------------------------------------------------------------

void cLuxPlayerState_UseItem::ImplementedUpdate(float afTimeStep)
{
	mFlashOscill.Update(afTimeStep);

	mbOtherPlayerInFocus = CheckOtherPlayerInFocus();
}

//-----------------------------------------------------------------------

cGuiGfxElement* cLuxPlayerState_UseItem::GetCrosshair()
{
	if(mpCurrentItem==NULL) return NULL;

	return mpCurrentItem->GetImage();
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_UseItem::OnDrawCrossHair(cGuiGfxElement *apGfx, const cVector3f& avPos, const cVector2f &avSize)
{
	// Draw on the owning player's hud set (P2's glow belongs on P2's viewport).
	cGuiSet *pHudSet = gpBase->mpGameHudSet;
	if(mpPlayer->IsPlayer2() && gpBase->mpMapHandler->GetCoopHudSet())
		pHudSet = gpBase->mpMapHandler->GetCoopHudSet();

	////////////////////////////////
	// Coop: the other player can receive the item — show the interact glow.
	if(mbOtherPlayerInFocus)
	{
		cVector3f vNewPos = avPos;
		vNewPos.z += 1;

		for(int i=0; i<3; ++i)
			pHudSet->DrawGfx(apGfx, vNewPos, avSize, cColor(mFlashOscill.val), eGuiMaterial_Additive);
		return true;
	}

	if(mpEntityInFocus==NULL) return true;

	float fMaxFocusDistance = cMath::Max(mpEntityInFocus->GetMaxFocusDistance(), mfMinUseItemDistance);
	if(mfFocusDistance > fMaxFocusDistance) return true;

	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(	mpEntityInFocus->GetEntityType() == eLuxEntityType_Prop)
	{
		iLuxProp *pProp = static_cast<iLuxProp*>(mpEntityInFocus);
		if(pProp->GetPropType() == eLuxPropType_Item) return true;
	}
	else
	{
		if(pMap->GetUseItemCallback(mpCurrentItem->GetName(), mpEntityInFocus->GetName())==NULL) return true;
	}

	cVector3f vNewPos = avPos;
	vNewPos.z += 1;

	for(int i=0; i<3; ++i)
		pHudSet->DrawGfx(apGfx, vNewPos, avSize, cColor(mFlashOscill.val), eGuiMaterial_Additive);
	return true;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxPlayerState_UseItem::ShowOutlineOnEntity(iLuxEntity *apEntity, iPhysicsBody *apBody, const cVector3f &avFocusPos)
{
	return false;

	/*cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL) return false;
	
	/////////////////////////////////
	// Tinder box
	if(mpCurrentItem->GetType() == eLuxItemType_Tinderbox)
	{
		if(apEntity->GetEntityType()!=eLuxEntityType_Prop) return false;

		iLuxProp *pProp = static_cast<iLuxProp*>(apEntity);
		if(pProp->GetPropType() !=eLuxPropType_Lamp) return false;

		cLuxProp_Lamp *pLamp = static_cast<cLuxProp_Lamp*>(pProp);
		return pLamp->CanBeIgnitByPlayer();
	}
	/////////////////////////////////
	// Puzzle item
	else
	{
		if(apEntity->CanInteract(apBody))
		{
			if(apEntity->GetEntityType() == eLuxEntityType_Prop)
			{
				iLuxProp *pProp = static_cast<iLuxProp*>(apEntity);
				if(pProp->GetPropType() != eLuxPropType_Item) return true;
			}
			else
			{
				return true;
			}
		}

		return pMap->GetUseItemCallback(mpCurrentItem->GetName(), apEntity->GetName())!=NULL;
	}*/
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_UseItem::CheckOtherPlayerInFocus()
{
	if(gpBase->mpMapHandler->GetCoopMode()==false) return false;

	// State updates run outside the interaction swap, so gpBase->mpPlayer
	// reliably holds Player 1 here.
	cLuxPlayer *pOther = mpPlayer->IsPlayer2() ? gpBase->mpPlayer : gpBase->mpPlayer2;
	if(pOther==NULL || pOther==mpPlayer || pOther->IsDead()) return false;

	iCharacterBody *pOtherBody = pOther->GetCharacterBody();
	cCamera *pCam = mpPlayer->GetCamera();
	if(pOtherBody==NULL || pCam==NULL) return false;

	////////////////////////////////
	// Close enough to hand something over?
	float fMaxDist = cMath::Max(2.1f, mfMinUseItemDistance);
	cVector3f vOtherCenter = pOtherBody->GetPosition();
	if(cMath::Vector3Dist(pCam->GetPosition(), vOtherCenter) > fMaxDist) return false;

	////////////////////////////////
	// Crosshair aimed at them? (view ray vs their body's bounding box)
	cBoundingVolume *pBV = pOtherBody->GetCurrentBody()->GetBoundingVolume();
	cVector3f vStart = pCam->GetPosition();
	cVector3f vEnd = vStart + pCam->GetForward() * fMaxDist;
	if(cMath::CheckAABBLineIntersection(pBV->GetMin(), pBV->GetMax(), vStart, vEnd, NULL, NULL)==false)
		return false;

	////////////////////////////////
	// And no wall between us.
	return gpBase->mpMapHelper->CheckLineOfSight(vStart, vOtherCenter, false);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_UseItem::GiveItemToOtherPlayer()
{
	if(mpCurrentItem==NULL) return;

	// Coop: with Full Item Share the inventory is shared, so giving general
	// items is disabled (no need). The lantern is exempt (separate toggle).
	if(gpBase->mpMapHandler->GetCoopMode() && ImGuiDebugMenu::GetFullItemShare() &&
	   mpCurrentItem->GetType() != eLuxItemType_Lantern)
	{
		gpBase->mpMessageHandler->SetMessage(_W("Inventory is shared - no need to give items."), 0, mpPlayer->GetPlayerIndex());
		return;
	}

	cLuxPlayer *pReceiver = mpPlayer->IsPlayer2() ? gpBase->mpPlayer : gpBase->mpPlayer2;
	if(pReceiver==NULL || pReceiver==mpPlayer) return;

	cLuxInventory *pInventory = gpBase->mpInventory;

	////////////////////////////////
	// Snapshot everything BEFORE removal (RemoveItem may delete the object).
	tString sName = mpCurrentItem->GetName();
	eLuxItemType type = mpCurrentItem->GetType();
	tString sSubType = mpCurrentItem->GetSubType();
	tString sImage = mpCurrentItem->GetImageName();
	float fAmount = mpCurrentItem->GetAmount();
	tString sVal = mpCurrentItem->GetStringVal();
	tString sExtraVal = mpCurrentItem->GetExtraStringVal();
	tString sGameNameEntry = mpCurrentItem->GetGameNameEntry();
	tString sGameDescEntry = mpCurrentItem->GetGameDescEntry();
	tWString sDisplayName = kTranslate("Inventory", sGameNameEntry);

	////////////////////////////////
	// Take it from the giver (handles stacked counts and equip pointers).
	pInventory->RemoveItem(mpCurrentItem);
	mpCurrentItem = NULL;

	////////////////////////////////
	// Handing over a LIT lantern has to snuff the giver's flame. cLuxPlayerLantern
	// only validates ownership inside SetActive(), so losing the item does not by
	// itself turn the light off — without this the giver walks away still glowing
	// from a lantern they no longer own. Pass abCheckForOilAndItems=false since the
	// item is already gone, and abCheckIfAllowed=false so the current state cannot
	// veto it. Skipped when Lantern Per Player is on: there both players may use the
	// lantern regardless of who holds the item.
	if(type == eLuxItemType_Lantern && ImGuiDebugMenu::GetLanternPerPlayer()==false)
	{
		mpPlayer->GetHelperLantern()->SetActive(false, true, false, false);
	}

	////////////////////////////////
	// Hand it to the receiver — AddItem routes items to P2's list while
	// gpBase->mpPlayer is swapped to P2 (the codebase's standard idiom).
	cLuxPlayer *pOrigPlayer = gpBase->mpPlayer;
	if(pReceiver->IsPlayer2()) gpBase->mpPlayer = gpBase->mpPlayer2;

	cLuxInventory_Item *pNewItem = pInventory->AddItem(sName, type, sSubType, sImage, fAmount, sVal, sExtraVal);
	if(pNewItem)
	{
		pNewItem->SetGameNameEntry(sGameNameEntry);
		pNewItem->SetGameDescEntry(sGameDescEntry);
	}

	gpBase->mpPlayer = pOrigPlayer;

	////////////////////////////////
	// Feedback: the giver reads what happened, the receiver hears the
	// familiar pickup sound.
	gpBase->mpMessageHandler->SetMessage(
		_W("Gave ") + sDisplayName + (pReceiver->IsPlayer2() ? _W(" to Player 2") : _W(" to Player 1")),
		0, mpPlayer->GetPlayerIndex());

	gpBase->mpHelpFuncs->PlayGuiSoundData("ui/pick_Generic", eSoundEntryType_World);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_UseItem::UseItem()
{
	if(mpCurrentItem==NULL) return;

	// Coop: everything below acts on "the player" -- the map script's use-item
	// callback and both failure messages. This ran unswapped, so a door P2
	// unlocked handed the script's sanity boost (and anything else it does to
	// the player) to P1, and "that doesn't work" appeared on P1's screen.
	// Point gpBase->mpPlayer at whoever is actually holding the item.
	cLuxPlayer *pPrevActing = cLuxPlayer::CoopBeginActAs(mpPlayer);

	iLuxEntity *pEntity = mpEntityInFocus;
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();

	/////////////////////////
	//An object is in focus
	if(pEntity && pMap && mfFocusDistance < cMath::Max(pEntity->GetMaxFocusDistance(), mfMinUseItemDistance))
	{
		cLuxUseItemCallback *pCallback = pMap->GetUseItemCallback(mpCurrentItem->GetName(), pEntity->GetName());
		if(pCallback)
		{
            // Running the script MAY destroy this item so "Backup" the check flag.
            bool bAutoDestroy = pCallback->mbAutoDestroy;
			tString sName = pCallback->msName;
            pMap->RunScript(pCallback->msFunction+ "(\"" + pCallback->msItem + "\", \"" + pCallback->msEntity + "\")" );

			if(bAutoDestroy)
			{
				pMap->RemoveUseItemCallback(pCallback, sName);
			}
		}
		else
		{
			gpBase->mpMessageHandler->SetMessage(kTranslate("Inventory","UseItemDoesNotWork"), 0);
		}
	}
	/////////////////////////
	//No object in focus
	else
	{
		gpBase->mpMessageHandler->SetMessage(kTranslate("Inventory","UseItemHasNoObject"), 0);
	}

	cLuxPlayer::CoopEndActAs(pPrevActing);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxPlayerState_UseItem_SaveData, iLuxPlayerState_DefaultBase_SaveData)
kEndSerialize()

//-----------------------------------------------------------------------

iLuxPlayerState_SaveData* cLuxPlayerState_UseItem::CreateSaveData()
{
	return hplNew(cLuxPlayerState_UseItem_SaveData, ());
}

//-----------------------------------------------------------------------


void cLuxPlayerState_UseItem::SaveToSaveData(iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::SaveToSaveData(apSaveData);
	cLuxPlayerState_UseItem_SaveData *pData = static_cast<cLuxPlayerState_UseItem_SaveData*>(apSaveData);


	///////////////////////
	// Save vars
}

//-----------------------------------------------------------------------

void cLuxPlayerState_UseItem::LoadFromSaveDataBeforeEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::LoadFromSaveDataBeforeEnter(apMap,apSaveData);
	cLuxPlayerState_UseItem_SaveData *pData = static_cast<cLuxPlayerState_UseItem_SaveData*>(apSaveData);

	///////////////////////
	// Load vars
}

//-----------------------------------------------------------------------

void cLuxPlayerState_UseItem::LoadFromSaveDataAfterEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::LoadFromSaveDataAfterEnter(apMap,apSaveData);
	cLuxPlayerState_UseItem_SaveData *pData = static_cast<cLuxPlayerState_UseItem_SaveData*>(apSaveData);

	///////////////////////
	// Load vars
}

//-----------------------------------------------------------------------


