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

#ifndef LUX_MAIN_MENU_START_GAME_H
#define LUX_MAIN_MENU_START_GAME_H

//----------------------------------------------

#include "LuxMainMenu.h"

//----------------------------------------------

/**
 * The modes the Start New Game window offers.
 *
 * The three co-op entries all force co-op onto the single-player main story.
 * Normal and Hard additionally apply a preset to the forced-coop compatibility
 * options; Custom leaves whatever the player set in Options -> Coop alone.
 */
enum eLuxStartMode
{
	eLuxStartMode_Normal = 0,
	eLuxStartMode_Hard,
	eLuxStartMode_CoopNormal,
	eLuxStartMode_CoopHard,
	eLuxStartMode_CoopCustom,

	eLuxStartMode_LastEnum,
};

class cLuxMainMenu_StartGame : public iLuxMainMenuWindow
{
public:
	cLuxMainMenu_StartGame(cGuiSet *apGuiSet, cGuiSkin *apGuiSkin);
	~cLuxMainMenu_StartGame();

	virtual void CreateGui();

	virtual void ExitPressed();

private:
	virtual void OnSetActive(bool abX);

	////////////////////////
	// Properties
	cVector2f mvWindowSize;

	////////////////////////
	// Layout
	cWidgetButton* mvModeButtons[eLuxStartMode_LastEnum];

	iWidget *mpStartButton;

	//Description
	cWidgetLabel* mpLDescription;

	////////////////////////
	// Which mode is currently picked out in the left-hand column.
	int mlSelectedMode;

	/** Highlight the picked mode, and put its blurb in the description panel. */
	void SetSelectedMode(int alMode);

	/** True for the three co-op entries. */
	bool IsCoopMode(int alMode) const;

	/**
	 * Write the chosen mode's forced-coop preset into the config handler and
	 * push it out. Does nothing for Custom, which is what Custom is for.
	 */
	void ApplyCoopPreset(int alMode);

	/** The confirmation the game has always shown before wiping a profile. */
	void ShowStartConfirmation();

	////////////////////////
	// Callbacks
	bool WindowOnUpdate(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(WindowOnUpdate);

	bool PressStartGame(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(PressStartGame);

	bool PressCancel(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(PressCancel);

	bool UIPressStart(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(UIPressStart);

	bool UIPressCancel(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(UIPressCancel);

	bool ExitCallback(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(ExitCallback);

	//MODE SELECTION
	//
	//One callback for all five buttons; which one was pressed is read back from
	//the widget's user value rather than needing a callback each.
	bool PressMode(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(PressMode);

	bool UIPressMode(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(UIPressMode);

	bool ClickedStartGamePopup(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(ClickedStartGamePopup);

	/** OK on the one-time "you are forcing co-op onto the main story" box. */
	bool ClickedForcedCoopWarning(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(ClickedForcedCoopWarning);
};

//----------------------------------------------

#endif // LUX_MAIN_MENU_START_GAME_H