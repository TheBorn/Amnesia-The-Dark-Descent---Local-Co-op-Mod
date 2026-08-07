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

#include "LuxMainMenu_StartGame.h"
#include "LuxBase.h"
#include "LuxInputHandler.h"
#include "LuxDebugHandler.h"
#include "LuxConfigHandler.h"

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxMainMenu_StartGame::cLuxMainMenu_StartGame(cGuiSet *apGuiSet, cGuiSkin *apGuiSkin) : iLuxMainMenuWindow(apGuiSet, apGuiSkin)
{
	//Wider and taller than the two-button original: five mode buttons down the
	//left and a description panel beside them that has to hold a paragraph.
	mvWindowSize = cVector2f(600, 320);
#if MAC_OS || LINUX
	mpStartButton = 0;
#else
    mpStartButton = nullptr;
#endif

	mlSelectedMode = eLuxStartMode_Normal;

	for(int i=0; i<eLuxStartMode_LastEnum; ++i)
		mvModeButtons[i] = NULL;
}

//-----------------------------------------------------------------------

cLuxMainMenu_StartGame::~cLuxMainMenu_StartGame()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxMainMenu_StartGame::CreateGui()
{
	//////////////////////////
	//Window
	mpWindow = mpGuiSet->CreateWidgetWindow(eWidgetWindowButtonFlag_None,cVector3f(0,0,5),mvWindowSize, kTranslate("MainMenu", "Start Game"));
	mpWindow->AddCallback(eGuiMessage_OnUpdate, this, kGuiCallback(WindowOnUpdate));

	float fBorderSize = 15;
	float fButtonWidth = 190;
	float fButtonHeight = 30.0f;
	float fButtonSepp = 3;

	//////////////////////////
	// Mode buttons, one column down the left.
	struct cModeDesc
	{
		const char *msEntry;
		const wchar_t *msLabel;
	};

	cModeDesc vModes[eLuxStartMode_LastEnum] =
	{
		{ "NormalMode",     _W("Normal") },
		{ "HardMode",       _W("Hard mode") },
		{ "CoopNormalMode", _W("Co-op Normal") },
		{ "CoopHardMode",   _W("Co-op Hard mode") },
		{ "CoopCustomMode", _W("Co-op Custom") },
	};

	cVector3f vButtonPosition = cVector3f(fBorderSize, 30 + fBorderSize + 10, 0.1f);

	for(int i=0; i<eLuxStartMode_LastEnum; ++i)
	{
		cWidgetButton *pModeButton = mpGuiSet->CreateWidgetButton(vButtonPosition,
			cVector2f(fButtonWidth, fButtonHeight),
			kTranslateOr("MainMenu", vModes[i].msEntry, vModes[i].msLabel), mpWindow);

		pModeButton->SetUserValue(i);
		pModeButton->AddCallback(eGuiMessage_ButtonPressed, this, kGuiCallback(PressMode));
		pModeButton->AddCallback(eGuiMessage_UIButtonPress, this, kGuiCallback(UIPressMode));

		mvModeButtons[i] = pModeButton;

		vButtonPosition.y += fButtonHeight + fButtonSepp * 2;
	}

	//////////////////////////
	// Start Game / Cancel along the bottom.
	cVector3f vPos = cVector3f(fBorderSize, mpWindow->GetSize().y - 35, 0.1f);

	cWidgetButton* pButton = mpGuiSet->CreateWidgetButton(vPos, cVector2f(fButtonWidth, fButtonHeight),
		kTranslate("MainMenu", "Start Game"), mpWindow);
	pButton->AddCallback(eGuiMessage_ButtonPressed,this, kGuiCallback(PressStartGame));
	pButton->AddCallback(eGuiMessage_UIButtonPress,this, kGuiCallback(UIPressStart));
	mpStartButton = pButton;

	vPos.x += fButtonWidth + fButtonSepp * 2;

	cWidgetButton* pCancelButton = mpGuiSet->CreateWidgetButton(vPos, cVector2f(120, fButtonHeight),
		kTranslate("Global", "Cancel"), mpWindow);
	pCancelButton->AddCallback(eGuiMessage_ButtonPressed, this, kGuiCallback(PressCancel));
	pCancelButton->AddCallback(eGuiMessage_UIButtonPress, this, kGuiCallback(UIPressCancel));

	////////////////////////////////////////
	// Set up focus navigation: straight down the mode column, then the two
	// bottom buttons side by side.
	for(int i=0; i<eLuxStartMode_LastEnum; ++i)
	{
		if(i+1 < eLuxStartMode_LastEnum)
			mvModeButtons[i]->SetFocusNavigation(eUIArrow_Down, mvModeButtons[i+1]);
		else
			mvModeButtons[i]->SetFocusNavigation(eUIArrow_Down, mpStartButton);

		if(i > 0)
			mvModeButtons[i]->SetFocusNavigation(eUIArrow_Up, mvModeButtons[i-1]);
	}

	mpStartButton->SetFocusNavigation(eUIArrow_Up, mvModeButtons[eLuxStartMode_LastEnum-1]);
	mpStartButton->SetFocusNavigation(eUIArrow_Right, pCancelButton);
	pCancelButton->SetFocusNavigation(eUIArrow_Up, mvModeButtons[eLuxStartMode_LastEnum-1]);
	pCancelButton->SetFocusNavigation(eUIArrow_Left, mpStartButton);

	////////////////////////////////////////
	// Description
	cVector3f vDescriptionPos = cVector3f(fBorderSize + fButtonWidth + fButtonSepp * 4, 30 + fBorderSize, 0.1f);

	cVector2f vDescriptionSize = cVector2f(
		mvWindowSize.x - vDescriptionPos.x - fBorderSize,
		mpWindow->GetSize().y - vDescriptionPos.y - 50);

	mpLDescription = mpGuiSet->CreateWidgetLabel(vDescriptionPos, vDescriptionSize, _W(""), mpWindow);
	mpLDescription->SetDefaultFontColor(cColor(1, 1));
	mpLDescription->SetWordWrap(true);
	mpLDescription->SetTextAlign(eFontAlign_Center);
	mpLDescription->SetClipActive(true);
	mpLDescription->SetScrollSpeedMul(4.0f);
	mpLDescription->SetDrawBackGround(true);
	mpLDescription->SetBackGroundColor(cColor(0, 0.5f));

	SetSelectedMode(eLuxStartMode_Normal);
}

//-----------------------------------------------------------------------

void cLuxMainMenu_StartGame::ExitPressed()
{
	if (mpGuiSet->PopUpIsActive()) return;

	gpBase->mpMainMenu->SetWindowActive(eLuxMainMenuWindow_LastEnum);
}

//-----------------------------------------------------------------------

void cLuxMainMenu_StartGame::OnSetActive(bool abX)
{
	if (abX)
	{
		//cLuxMainMenu::PressStartGame clears mbHardMode before opening this
		//window, so re-assert whatever is picked instead of letting the two
		//disagree.
		SetSelectedMode(mlSelectedMode);

		mpGuiSet->SetDefaultFocusNavWidget(mpStartButton);
		mpGuiSet->SetFocusedWidget(mpStartButton);
	}
}

//-----------------------------------------------------------------------

bool cLuxMainMenu_StartGame::WindowOnUpdate(iWidget* apWidget, const cGuiMessageData& aData)
{
	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_StartGame, WindowOnUpdate);

//-----------------------------------------------------------------------

bool cLuxMainMenu_StartGame::IsCoopMode(int alMode) const
{
	return alMode == eLuxStartMode_CoopNormal ||
		   alMode == eLuxStartMode_CoopHard ||
		   alMode == eLuxStartMode_CoopCustom;
}

//-----------------------------------------------------------------------

void cLuxMainMenu_StartGame::SetSelectedMode(int alMode)
{
	if(alMode < 0 || alMode >= eLuxStartMode_LastEnum) return;

	mlSelectedMode = alMode;

	//The picked entry is the yellow one, exactly as Normal/Hard always were.
	const cColor cPicked(232.0f / 255.0f, 201.0f / 255.0f, 28.0f / 255.0f, 1.0f);
	const cColor cUnpicked(1.f, 1.0f);

	for(int i=0; i<eLuxStartMode_LastEnum; ++i)
	{
		if(mvModeButtons[i]==NULL) continue;
		mvModeButtons[i]->SetDefaultFontColor(i==alMode ? cPicked : cUnpicked);
	}

	//Hard mode is a property of the story, not of co-op: both the single-player
	//Hard entry and Co-op Hard set it.
	gpBase->mbHardMode = (alMode == eLuxStartMode_Hard || alMode == eLuxStartMode_CoopHard);

	static const char *vDescEntries[eLuxStartMode_LastEnum] =
	{
		"NormalModeDescription",
		"HardModeDescription",
		"CoopNormalModeDescription",
		"CoopHardModeDescription",
		"CoopCustomModeDescription",
	};

	static const wchar_t *vDescDefaults[eLuxStartMode_LastEnum] =
	{
		_W(""),
		_W(""),
		_W("Simpler co-op gameplay, easier but less involved for player 2."),
		_W("Resources has to be conserved between the 2 very carefully, item effects are not shared and only 1 lantern exists, swap it between each other or stay close!"),
		_W("Co-op with the compatibility options exactly as you set them in Options -> Coop. Nothing is preset for you."),
	};

	mpLDescription->SetText(kTranslateOr("MainMenu", vDescEntries[alMode], vDescDefaults[alMode]));
}

//-----------------------------------------------------------------------

void cLuxMainMenu_StartGame::ApplyCoopPreset(int alMode)
{
	//Custom is the escape hatch: it deliberately leaves every option alone.
	if(alMode != eLuxStartMode_CoopNormal && alMode != eLuxStartMode_CoopHard) return;

	cLuxConfigHandler *pCfg = gpBase->mpConfigHandler;
	const bool bNormal = (alMode == eLuxStartMode_CoopNormal);

	//Shared in both presets -- these are the ones that stop a single-player
	//story from simply losing Player 2, rather than making it easier.
	pCfg->mbCoopScriptTeleportBoth = true;
	pCfg->mbCoopSharedFlashbacks   = true;
	pCfg->mbCoopSharedSanityReward = true;
	pCfg->mbCoopP2Callbacks        = true;

	//Normal pools everything; Hard makes them ration it. The one thing Hard
	//still shares is the lantern -- there is only ever one, which is what makes
	//it a resource to hand back and forth.
	pCfg->mbCoopSharedLantern           = true;
	pCfg->mbCoopShareOilEffect          = bNormal;
	pCfg->mbCoopShareLaudanumEffect     = bNormal;
	pCfg->mbCoopShareSanityPotionEffect = bNormal;
	pCfg->mbCoopShareTinderboxes        = bNormal;
	pCfg->mbCoopFullItemShare           = bNormal;

	//Both players at the level door is the hard-mode recommendation.
	pCfg->mbCoopBothAtLevelDoor = (bNormal==false);

	pCfg->ApplyDebugAndCoopSettings();
}

//-----------------------------------------------------------------------

void cLuxMainMenu_StartGame::ShowStartConfirmation()
{
	//Re-assert rather than trust: cLuxMainMenu::PressStartGame sets mbHardMode
	//false immediately AFTER making this window active, so whatever
	//OnSetActive put there has already been overwritten by the time we get here.
	gpBase->mbHardMode = (mlSelectedMode == eLuxStartMode_Hard || mlSelectedMode == eLuxStartMode_CoopHard);

	tWString sDesciption = gpBase->mbHardMode == false ? kTranslate("MainMenu", "Start a new game?") : kTranslate("MainMenu", "HardModeStartNewGame");

	cGuiPopUpMessageBox *pPopUp = mpGuiSet->CreatePopUpMessageBox(_W(""), sDesciption,
		kTranslate("MainMenu", "Yes"), kTranslate("MainMenu", "No"),
		this,
		kGuiCallback(ClickedStartGamePopup));
	
	pPopUp->GetGuiSet()->SetDrawFocus(mpGuiSet->GetDrawFocus());
	pPopUp->SetKillOnEscapeKey(false);
}

//-----------------------------------------------------------------------

bool cLuxMainMenu_StartGame::PressMode(iWidget* apWidget, const cGuiMessageData& aData)
{
	SetSelectedMode(apWidget->GetUserValue());

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_StartGame, PressMode);

//-----------------------------------------------------------------------

bool cLuxMainMenu_StartGame::UIPressMode(iWidget* apWidget, const cGuiMessageData& aData)
{
	switch (aData.mlVal)
	{
	case eUIButton_Primary:	return PressMode(apWidget, aData);
	case eUIButton_Secondary: return PressCancel(apWidget, aData);
	}

	return false;
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_StartGame, UIPressMode);

//-----------------------------------------------------------------------

bool cLuxMainMenu_StartGame::ClickedForcedCoopWarning(iWidget* apWidget, const cGuiMessageData& aData)
{
	bool bOkPressed = aData.mlVal == 0 ? true : false;

	if(bOkPressed==false)
	{
		//Backed out. Deliberately NOT marked as accepted -- a warning you
		//declined has not been acknowledged, so it comes back next time.
		mpGuiSet->SetDrawFocus(false);
		return true;
	}

	gpBase->mpConfigHandler->mbCoopForcedWarningAccepted = true;

	//Straight on to the usual "start a new game?" confirmation, so the two
	//boxes read as one flow rather than the warning cancelling the press.
	ShowStartConfirmation();

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_StartGame, ClickedForcedCoopWarning);

//-----------------------------------------------------------------------

bool cLuxMainMenu_StartGame::ClickedStartGamePopup(iWidget* apWidget, const cGuiMessageData& aData)
{
	bool bStartGame = aData.mlVal == 0 ? true : false;
	mpGuiSet->SetDrawFocus(false);

	if (bStartGame)
	{
		//Decided here, read by cLuxBase::StartGame just before the first map.
		gpBase->mbHardMode   = (mlSelectedMode == eLuxStartMode_Hard || mlSelectedMode == eLuxStartMode_CoopHard);
		gpBase->mbCoopWanted = IsCoopMode(mlSelectedMode);
		ApplyCoopPreset(mlSelectedMode);

		gpBase->SetCustomStory(NULL);
		gpBase->mpMainMenu->ExitMenu(eLuxMainMenuExit_StartGame);
		gpBase->mpMainMenu->SetWindowActive(eLuxMainMenuWindow_LastEnum);
	}

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_StartGame, ClickedStartGamePopup);

//-----------------------------------------------------------------------

bool cLuxMainMenu_StartGame::PressStartGame(iWidget* apWidget, const cGuiMessageData& aData)
{
	mpGuiSet->SetDrawFocus(gpBase->mpInputHandler->IsGamepadPresent());

	//////////////////////////////////////////////////////////////
	// Forcing co-op onto the main story is worth saying out loud, once.
	if(IsCoopMode(mlSelectedMode) && gpBase->mpConfigHandler->mbCoopForcedWarningAccepted==false)
	{
		cGuiPopUpMessageBox *pWarning = mpGuiSet->CreatePopUpMessageBox(
			kTranslateOr("MainMenu","ForcedCoopWarningLabel", _W("Forced co-op")),
			kTranslateOr("MainMenu","ForcedCoopWarningMessage",
				_W("You are about to enter the main story in co-op mode, this will force co-op on a single player based game, things were not meant to be this way in Amnesia, there may be bugs, unexpected behaviour or soft locks. Player 2 can be teleported to player 1 with F2 and player 1 can teleport to player 2 with F1. Pressing both joysticks buttons down on the controller will teleport player 2 to player 1. Noclip can be used by player 1 by pressing V at any time. For now these debug teleports and noclips are on by default to allow players to get out of potential soft locks, in the future they might need separate toggle in the debug sections. These features are off by default in co-op supported custom stories.")),
			kTranslate("MainMenu","OK"), kTranslate("Global","Cancel"),
			this, kGuiCallback(ClickedForcedCoopWarning), 760);

		pWarning->GetGuiSet()->SetDrawFocus(mpGuiSet->GetDrawFocus());
		pWarning->SetKillOnEscapeKey(false);

		return true;
	}

	ShowStartConfirmation();

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_StartGame, PressStartGame);


//-----------------------------------------------------------------------

bool cLuxMainMenu_StartGame::PressCancel(iWidget* apWidget, const cGuiMessageData& aData)
{
	ExitCallback(NULL, cGuiMessageData(0));
	return true; 
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_StartGame, PressCancel);

//-----------------------------------------------------------------------

bool cLuxMainMenu_StartGame::ExitCallback(iWidget* apWidget, const cGuiMessageData& aData)
{
	bool bOkPressed = aData.mlVal == 0 ? true : false;
	if (bOkPressed == false)
		return true;

	if (gpBase->mpCustomStory == NULL)
		gpBase->mpMainMenu->SetWindowActive(eLuxMainMenuWindow_LastEnum);
	else
		gpBase->mpMainMenu->SetWindowActive(eLuxMainMenuWindow_CustomStory);

	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_StartGame, ExitCallback);

//-----------------------------------------------------------------------

bool cLuxMainMenu_StartGame::UIPressStart(iWidget* apWidget, const cGuiMessageData& aData)
{
	switch(aData.mlVal)
	{
	case eUIButton_Primary:	return PressStartGame(apWidget, aData);
	case eUIButton_Secondary: return PressCancel(apWidget, aData);
	}

	return false;
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_StartGame, UIPressStart);

bool cLuxMainMenu_StartGame::UIPressCancel(iWidget* apWidget, const cGuiMessageData& aData)
{
	switch(aData.mlVal)
	{
	case eUIButton_Primary: return PressCancel(apWidget, aData);
	case eUIButton_Secondary: return PressCancel(apWidget, aData);
	}

	return false;
}
kGuiCallbackDeclaredFuncEnd(cLuxMainMenu_StartGame, UIPressCancel);

//-----------------------------------------------------------------------

