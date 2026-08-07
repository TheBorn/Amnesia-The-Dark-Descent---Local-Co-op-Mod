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

#ifndef LUX_MAIN_MENU_CUSTOM_STORY_H
#define LUX_MAIN_MENU_CUSTOM_STORY_H

//----------------------------------------------

#include "LuxMainMenu.h"

//----------------------------------------------

class cLuxCustomStorySettings;

//----------------------------------------------

class cLuxMainMenu_CustomStory : public iLuxMainMenuWindow
{
public:
	cLuxMainMenu_CustomStory(cGuiSet *apGuiSet, cGuiSkin *apGuiSkin);
	~cLuxMainMenu_CustomStory();

	void CreateGui();

	void ExitPressed() {}

	void SetCurrentStory(cLuxCustomStorySettings* apStory);

private:
	void OnSetActive(bool abX);

	bool PressContinue(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(PressContinue);
	
	bool PressStart(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(PressStart);

	bool PressStartCoop(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(PressStartCoop);

	/**
	 * OK on the "this story was not designed for co-op" box.
	 *
	 * Unlike the main game's equivalent this is NOT a one-time warning -- it is
	 * about the story in front of you, so it appears every time a story that has
	 * not declared co-op support is started in co-op.
	 */
	bool ClickedStoryCoopWarning(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(ClickedStoryCoopWarning);

	/** Everything both start paths share, once the warning is out of the way. */
	void BeginStory(bool abCoop);

	bool PressLoadGame(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(PressLoadGame);

	bool PressBack(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(PressBack);

	void RepositionButtons();

	cVector2f mvWindowSize;

	cLuxCustomStorySettings* mpStory;

	cWidgetImage* mpIPicture;

	cWidgetLabel* mpLAuthor;
	cWidgetLabel* mpLDesc;

	std::vector<cWidgetButton*> mvButtons;
};

//----------------------------------------------

class cLuxMainMenu_CustomStoryList : public iLuxMainMenuWindow
{
public:
	cLuxMainMenu_CustomStoryList(cGuiSet *apGuiSet, cGuiSkin *apGuiSkin, cLuxMainMenu_CustomStory* apWindow);
	~cLuxMainMenu_CustomStoryList();

	void CreateGui();

	void ExitPressed();

private:
	void OnSetActive(bool abX);

	void PopulateStoryList();
#ifdef USERDIR_RESOURCES
	void PopulateUserDirStoryList();
#endif

	/**
	 * Add every valid story found directly under asPath.
	 *
	 * abTryOneLevelDeeper is for Steam Workshop items: the item folder is a numeric
	 * id, and most uploaders put custom_story_settings.cfg at the top of it while
	 * some wrap the story in one more subfolder. A folder that fails as a story is
	 * then retried one level in -- one level, not a recursive walk, which over a
	 * workshop folder would be slow and would start finding maps directories.
	 */
	void AddStoriesFromDir(const tWString& asPath, bool abTryOneLevelDeeper = false);

	void ClearStoryList();

	////////////////////////
	// Properties
	cVector2f mvWindowSize;

	cLuxMainMenu_CustomStory* mpStoryWindow;

	////////////////////////
	// Layout
	cWidgetListBox* mpLBStories;

	////////////////////////
	// Callbacks
	void LoadStory(int alIdx);

	bool WindowOnUpdate(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(WindowOnUpdate);

	bool SelectStory(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(SelectStory);

	bool PressOK(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(PressOK);

	bool PressCancel(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(PressCancel);

	bool LoadStoryCallback(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(LoadStoryCallback);

	bool ExitCallback(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(ExitCallback);
};

//----------------------------------------------

#endif // LUX_MAIN_MENU_CUSTOM_STORY_H
