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

#include "LuxItemType.h"

#include "LuxInventory.h"
#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"
#include "LuxPlayerState.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxEntity.h"
#include "LuxJournal.h"
#include "LuxCompletionCountHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxHintHandler.h"
#include "impl/ImGuiDebugMenu.h"


//////////////////////////////////////////////////////////////////////////
// INTERFACE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iLuxItemType::iLuxItemType(const tString& asName, eLuxItemType aType)
{
	msName = asName;
	mType = aType;
	mbHasCount = false;
	mbShowPickUpMessage = true;
}

//-----------------------------------------------------------------------

iLuxItemType::~iLuxItemType()
{
	
}

//-----------------------------------------------------------------------

void iLuxItemType::AddCompletionAmount(int alAmount)
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL) return;

	pMap->AddCompletionAmount(alAmount);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUZZLE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxItemType_Puzzle::cLuxItemType_Puzzle() : iLuxItemType("Puzzle", eLuxItemType_Puzzle)
{
	
}


//-----------------------------------------------------------------------

bool cLuxItemType_Puzzle::BeforeAddItem(cLuxInventory_Item *apItem)
{
	ProgLog(eLuxProgressLogLevel_Medium, "Picked up puzzle item "+ apItem->GetName());

	AddCompletionAmount(gpBase->mpCompletionCountHandler->mlItemCompletionValue);
	return false;
}

void cLuxItemType_Puzzle::OnUse(cLuxInventory_Item *apItem, int alSlotIndex)
{
	cLuxPlayer *pUser = gpBase->mpInventory->GetActivePlayer();
	if(pUser->GetCurrentState()==eLuxPlayerState_UseItem)
	{
		pUser->ChangeState(eLuxPlayerState_Normal);
	}

	cLuxPlayerStateVars::SetupUseItem(apItem);

	pUser->ChangeState(eLuxPlayerState_UseItem);

	gpBase->mpInventory->ExitPressed();
}


//////////////////////////////////////////////////////////////////////////
// COINS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxItemType_Coins::cLuxItemType_Coins() : iLuxItemType("Coins", eLuxItemType_Coins)
{

}

//-----------------------------------------------------------------------

bool cLuxItemType_Coins::BeforeAddItem(cLuxInventory_Item *apItem)
{
	int lCoins = gpBase->mpPlayer->GetCoins();
	lCoins += (int)apItem->GetAmount();
	gpBase->mpPlayer->SetCoins(lCoins);

	gpBase->mpHintHandler->Add("PickCoin", kTranslate("Hints", "PickCoin"), 0);

	return true;
}

//-----------------------------------------------------------------------

void cLuxItemType_Coins::OnUse(cLuxInventory_Item *apItem, int alSlotIndex)
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// NOTE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxItemType_Note::cLuxItemType_Note() : iLuxItemType("Note", eLuxItemType_Note)
{
	mbShowPickUpMessage = false;
}

//-----------------------------------------------------------------------

bool cLuxItemType_Note::BeforeAddItem(cLuxInventory_Item *apItem)
{
	//First thing, and unconditionally: the latch belongs to THIS pickup and must
	//not survive it, whichever way the function leaves.
	const bool bReadByBoth = gpBase->mpJournal->ConsumeNextNoteReadByBoth();

	cLuxNote *pNote = gpBase->mpJournal->AddNote(apItem->GetStringVal(), apItem->GetImageName());
	if(pNote==NULL)return true;

	ProgLog(eLuxProgressLogLevel_Medium, "Picked up note "+ apItem->GetStringVal());

	if(apItem->GetAmount() > 0)
	{
		//////////////////////////////////////////////////////////////////////
		// WHO READS THIS.
		//
		// FORCED co-op is a single-player story two people are playing anyway.
		// Nothing in it was written for two readers and there is no author to
		// ask, so a note found in the world is treated as a thing that happened
		// to the PARTY: the game stops, both of them read it, and either of them
		// may turn the page or put it down.
		//
		// A story that DECLARES co-op support gets the opposite default, because
		// there somebody can decide. Only the player who picked it up reads it,
		// the world keeps running for the other one, and a note meant for the
		// pair is ticked "ReadByBoth" on the prop in the level editor -- which
		// then behaves exactly like the forced-co-op case.
		//
		// Outside co-op both flags are false and this is the original code path.
		const bool bShared = gpBase->mpMapHandler->GetCoopMode() &&
							(::ImGuiDebugMenu::IsNativeCoopStory()==false || bReadByBoth);

		//Latched for the CO-OP STATE readout: every one of these is cleared the
		//instant the journal closes, so without this there is nothing left to look
		//at afterwards and the only way to tell what happened is to guess.
		gpBase->mpJournal->SetLastNoteDecision(
			"note coop=" + cString::ToString((int)gpBase->mpMapHandler->GetCoopMode()) +
			" nativeStory=" + cString::ToString((int)::ImGuiDebugMenu::IsNativeCoopStory()) +
			" readByBoth=" + cString::ToString((int)bReadByBoth) +
			" -> shared=" + cString::ToString((int)bShared) +
			" picker=" + tString(gpBase->mpPlayer && gpBase->mpPlayer->IsPlayer2() ? "P2" : "P1"));

		gpBase->mpJournal->SetShowOnBothPlayers(bShared);
		gpBase->mpJournal->SetPauseBothPlayers(bShared);

		//gpBase->mpPlayer IS the picker here: an interaction by P2 runs inside
		//cLuxPlayer::CoopBeginActAs, which swaps them in for the duration. Not a
		//shortcut -- it is how every other per-player path in this file reads.
		gpBase->mpJournal->SetActivePlayer(gpBase->mpPlayer);
		gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
		gpBase->mpJournal->SetForceInstantExit(true);
		if(apItem->GetAmount() >= 2)
			gpBase->mpJournal->OpenNote(pNote, true);
		else
			gpBase->mpJournal->OpenNote(pNote, false);
	}

	AddCompletionAmount(gpBase->mpCompletionCountHandler->mlNoteCompletionValue);

	return true;
}

//-----------------------------------------------------------------------

void cLuxItemType_Note::OnUse(cLuxInventory_Item *apItem, int alSlotIndex)
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// DIARY
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxItemType_Diary::mbShowJournalOnPickup = true;

//-----------------------------------------------------------------------

cLuxItemType_Diary::cLuxItemType_Diary() : iLuxItemType("Diary", eLuxItemType_Diary)
{
	mbShowPickUpMessage = false;
}

//-----------------------------------------------------------------------

bool cLuxItemType_Diary::BeforeAddItem(cLuxInventory_Item *apItem)
{
	//See the note at the top of cLuxItemType_Note::BeforeAddItem.
	const bool bReadByBoth = gpBase->mpJournal->ConsumeNextNoteReadByBoth();

	int lDiaryIdx;
	cLuxDiary *pDiary = gpBase->mpJournal->AddDiary(apItem->GetStringVal(), apItem->GetImageName(), lDiaryIdx);
	if(pDiary==NULL) return true;

	ProgLog(eLuxProgressLogLevel_Medium, "Picked up diary "+ apItem->GetStringVal());

	mbShowJournalOnPickup = true;
	const tString &sCallbackFunc = apItem->GetExtraStringVal();
	if(sCallbackFunc != "")
	{
		cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	
		pMap->RunScript(sCallbackFunc+"(\""+apItem->GetName()+ "\","+ cString::ToString(lDiaryIdx)+")" );
	}

	if(mbShowJournalOnPickup)
	{
		//Coop: same rule as a note picked up in the world. See the long note in
		//cLuxItemType_Note::BeforeAddItem.
		const bool bShared = gpBase->mpMapHandler->GetCoopMode() &&
							(::ImGuiDebugMenu::IsNativeCoopStory()==false || bReadByBoth);

		gpBase->mpJournal->SetShowOnBothPlayers(bShared);
		gpBase->mpJournal->SetPauseBothPlayers(bShared);
		gpBase->mpJournal->SetActivePlayer(gpBase->mpPlayer);
		gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
		gpBase->mpJournal->SetForceInstantExit(true);
		gpBase->mpJournal->OpenDiary(pDiary, true);
	}
	else
	{
		gpBase->mpJournal->SetDiaryAsLastRead(pDiary);
	}

	AddCompletionAmount(gpBase->mpCompletionCountHandler->mlDiaryCompletionValue);

	return true;
}

//-----------------------------------------------------------------------

void cLuxItemType_Diary::OnUse(cLuxInventory_Item *apItem, int alSlotIndex)
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// LANTERN
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxItemType_Lantern::cLuxItemType_Lantern() : iLuxItemType("Lantern", eLuxItemType_Lantern)
{

}

//-----------------------------------------------------------------------

bool cLuxItemType_Lantern::BeforeAddItem(cLuxInventory_Item *apItem)
{
	ProgLog(eLuxProgressLogLevel_Medium, "Picked up latern");

	gpBase->mpHintHandler->Add("PickLantern", kTranslate("Hints", "PickLantern"), 0);

	return false;
}

//-----------------------------------------------------------------------

void cLuxItemType_Lantern::OnUse(cLuxInventory_Item *apItem, int alSlotIndex)
{
	cLuxPlayer *pUser = gpBase->mpInventory->GetActivePlayer();

	////////////////////////////////
	// Coop: when the players do NOT get a lantern each, the single lantern has to be
	// passable between them. Double-clicking it here only ever toggled the flame, so
	// it could never be taken out as a held item and handed over — the one item that
	// most needs handing over was the one item you could not hand over.
	//
	// Route it through eLuxPlayerState_UseItem like every other giveable item
	// (same shape as cLuxItemType_Puzzle::OnUse). Nothing is lost: the dedicated
	// lantern action still toggles the flame, and cLuxPlayerState_UseItem's
	// GiveItemToOtherPlayer() already exempts the lantern from the Full Item Share
	// block, so it works whether or not general items are shared.
	//
	// With Lantern Per Player ON both players may use the lantern regardless of who
	// holds the item, so there is nothing to hand over — keep the plain toggle.
	if(gpBase->mpMapHandler->GetCoopMode() && ImGuiDebugMenu::GetLanternPerPlayer()==false)
	{
		if(pUser->GetCurrentState()==eLuxPlayerState_UseItem)
			pUser->ChangeState(eLuxPlayerState_Normal);

		cLuxPlayerStateVars::SetupUseItem(apItem);
		pUser->ChangeState(eLuxPlayerState_UseItem);

		gpBase->mpInventory->ExitPressed();
		return;
	}

	cLuxPlayerLantern *pLantern = pUser->GetHelperLantern();
	pLantern->SetActive(!pLantern->IsActive(), true);
}

//-----------------------------------------------------------------------

tWString cLuxItemType_Lantern::GetDisplayedNameAdd(cLuxInventory_Item *apItem)
{
	cLuxPlayerLantern *pLantern = gpBase->mpInventory->GetActivePlayer()->GetHelperLantern();
	tWString sStr = pLantern->IsActive() ? kTranslate("Inventory", "LanternOn"): kTranslate("Inventory", "LanternOff");
	return _W(" (") +sStr + _W(")");
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// HEALTH
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxItemType_Health::cLuxItemType_Health() : iLuxItemType("Health", eLuxItemType_Health)
{
	mbHasCount = true;
	mlMaxCount = 99;
}

bool cLuxItemType_Health::BeforeAddItem(cLuxInventory_Item *apItem)
{
	gpBase->mpHintHandler->Add("PickHealthPotion", kTranslate("Hints", "PickHealthPotion"), 0);

	return false;
}

void cLuxItemType_Health::OnUse(cLuxInventory_Item *apItem, int alSlotIndex)
{
	cLuxPlayer *pUser = gpBase->mpInventory->GetActivePlayer();
	float fHealth = pUser->GetHealth();
	if(fHealth >= 100) return;

	fHealth += apItem->GetAmount();
	if(fHealth > 100) fHealth = 100;
	pUser->SetHealth(fHealth);

	// Coop: Shared Laudanum Effect — heal the other player too.
	if(gpBase->mpMapHandler->GetCoopMode() && ImGuiDebugMenu::GetShareLaudanumEffect())
	{
		cLuxPlayer *pOther = pUser->IsPlayer2() ? gpBase->mpPlayer : gpBase->mpPlayer2;
		if(pOther && pOther != pUser)
		{
			float fH2 = pOther->GetHealth();
			if(fH2 < 100) { fH2 += apItem->GetAmount(); if(fH2 > 100) fH2 = 100; pOther->SetHealth(fH2); }
		}
	}
	gpBase->mpInventory->RemoveItem(apItem);

	gpBase->mpHelpFuncs->PlayGuiSoundData("ui_use_health", eSoundEntryType_Gui);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// SANITY
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxItemType_Sanity::cLuxItemType_Sanity() : iLuxItemType("Sanity", eLuxItemType_Sanity)
{
	mbHasCount = true;
	mlMaxCount = 99;
}

bool cLuxItemType_Sanity::BeforeAddItem(cLuxInventory_Item *apItem)
{
	gpBase->mpHintHandler->Add("PickSanityPotion", kTranslate("Hints", "PickSanityPotion"), 0);

	return false;
}

void cLuxItemType_Sanity::OnUse(cLuxInventory_Item *apItem, int alSlotIndex)
{
	cLuxPlayer *pUser = gpBase->mpInventory->GetActivePlayer();
	float fSanity = pUser->GetSanity();
	if(fSanity >= 100) return;

	pUser->AddSanity(apItem->GetAmount());

	// Coop: Share Sanity Potion Effect — restore the other player too.
	if(gpBase->mpMapHandler->GetCoopMode() && ImGuiDebugMenu::GetShareSanityPotionEffect())
	{
		cLuxPlayer *pOther = pUser->IsPlayer2() ? gpBase->mpPlayer : gpBase->mpPlayer2;
		if(pOther && pOther != pUser && pOther->GetSanity() < 100) pOther->AddSanity(apItem->GetAmount(), false);
	}
	gpBase->mpInventory->RemoveItem(apItem);

	gpBase->mpHelpFuncs->PlayGuiSoundData("ui_use_sanity", eSoundEntryType_Gui);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// LAMP OIL
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxItemType_LampOil::cLuxItemType_LampOil()  : iLuxItemType("LampOil", eLuxItemType_LampOil)
{
	mbHasCount = true;
	mlMaxCount = 99;
}

bool cLuxItemType_LampOil::BeforeAddItem(cLuxInventory_Item *apItem)
{
	gpBase->mpHintHandler->Add("PickOil", kTranslate("Hints", "PickOil"), 0);

	return false;
}

void cLuxItemType_LampOil::OnUse(cLuxInventory_Item *apItem, int alSlotIndex)
{
	cLuxInventory *pInventory = gpBase->mpInventory;
	cLuxPlayer *pUser = pInventory->GetActivePlayer();
	bool bHasLantern = pInventory->HasItemOfType(eLuxItemType_Lantern);
	if(!bHasLantern && gpBase->mpMapHandler->GetCoopMode() && ImGuiDebugMenu::GetLanternPerPlayer())
		bHasLantern = pInventory->AnyPlayerHasItemOfType(eLuxItemType_Lantern);
	if(bHasLantern==false)
	{
		pInventory->SetMessageText(kTranslate("Inventory","OilNeedsLantern"),0);
		return;
	}

	float fLampOil = pUser->GetLampOil();
	if(fLampOil >= 100) return;

	fLampOil += apItem->GetAmount();
	if(fLampOil > 100) fLampOil = 100;
	pUser->SetLampOil(fLampOil);

	// Coop: Shared Oil Effect — top up the other player's lantern too.
	if(gpBase->mpMapHandler->GetCoopMode() && ImGuiDebugMenu::GetShareOilEffect())
	{
		cLuxPlayer *pOther = pUser->IsPlayer2() ? gpBase->mpPlayer : gpBase->mpPlayer2;
		if(pOther && pOther != pUser)
		{
			float fO2 = pOther->GetLampOil();
			if(fO2 < 100) { fO2 += apItem->GetAmount(); if(fO2 > 100) fO2 = 100; pOther->SetLampOil(fO2); }
		}
	}
	pInventory->RemoveItem(apItem);

	gpBase->mpHelpFuncs->PlayGuiSoundData("ui_use_oil", eSoundEntryType_Gui);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// TINDERBOX
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxItemType_Tinderbox::cLuxItemType_Tinderbox()  : iLuxItemType("Tinderbox", eLuxItemType_Tinderbox)
{
	mbHasCount = true;
	mlMaxCount = 99;
}

bool cLuxItemType_Tinderbox::GetHasMaxAmount()
{
	//if(gpBase->mpPlayer->GetTinderboxes()>=10) return true;
	return false;
}

bool cLuxItemType_Tinderbox::BeforeAddItem(cLuxInventory_Item *apItem)
{
	gpBase->mpHintHandler->Add("PickTinderbox", kTranslate("Hints", "PickTinderbox"), 0);

	gpBase->mpPlayer->AddTinderboxes(1);

	return true;
}

void cLuxItemType_Tinderbox::OnUse(cLuxInventory_Item *apItem, int alSlotIndex)
{
    cLuxPlayer *pUser = gpBase->mpInventory->GetActivePlayer();
    cLuxPlayerStateVars::SetupUseItem(apItem);

	pUser->ChangeState(eLuxPlayerState_UseItem);

	gpBase->mpInventory->ExitPressed();
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// HAND OBJECT
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxItemType_HandObject::cLuxItemType_HandObject() : iLuxItemType("HandObject", eLuxItemType_HandObject)
{

}

//-----------------------------------------------------------------------

bool cLuxItemType_HandObject::BeforeAddItem(cLuxInventory_Item *apItem)
{
	if(gpBase->mpInventory->GetEquippedHandItem() == NULL)
	{
		gpBase->mpInventory->SetEquippedHandItem(apItem);
	}

    return false;
}

//-----------------------------------------------------------------------

void cLuxItemType_HandObject::OnUse(cLuxInventory_Item *apItem, int alSlotIndex)
{
	gpBase->mpInventory->SetEquippedHandItem(apItem);
}

//-----------------------------------------------------------------------



