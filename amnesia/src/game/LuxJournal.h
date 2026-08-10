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

#ifndef LUX_JOURNAL_H
#define LUX_JOURNAL_H

//----------------------------------------------

#include "LuxBase.h"

class cLuxPlayer;

//----------------------------------------

enum eLuxJournalState
{
	eLuxJournalState_Main,
	eLuxJournalState_Notes,
	eLuxJournalState_Diaries,
	eLuxJournalState_QuestLog,
	eLuxJournalState_OpenNote,
	eLuxJournalState_OpenDiary,
	eLuxJournalState_OpenNarratedDiary,

	eLuxJournalState_LastEnum,
};

//----------------------------------------

class cLuxDiaryContainer
{
public:
	~cLuxDiaryContainer();

	tString msType;
	std::vector<cLuxDiary*> mvDiaries;
};
//----------------------------------------

class cLuxNotePage
{
public:
	tString msVoice;
	tWStringVec mvRows;
};

//----------------------------------------

class iLuxJournalWidgetData
{
public:
	iLuxJournalWidgetData(iWidget *apWidget) : mpWidget(apWidget) {}
	virtual ~iLuxJournalWidgetData(){}

	virtual void Update(float afTimeStep)=0;

	iWidget *mpWidget;
	void *mpExtraData;
};

typedef std::list<iLuxJournalWidgetData*> tLuxJournalWidgetData;
typedef tLuxJournalWidgetData::iterator tLuxJournalWidgetDataIt;

//----------------------------------------

class cLuxJournalTextData : public iLuxJournalWidgetData
{
public:	
	cLuxJournalTextData(iWidget *apWidget, eLuxJournalState aType) : iLuxJournalWidgetData(apWidget), mType(aType), mfEffectfAlpha(0){}

	void Update(float afTimeStep);

	eLuxJournalState mType;
	float mfEffectfAlpha;
};

//----------------------------------------

class cLuxJournal;

class cLuxJournalStateData
{
public:
	cLuxJournalStateData(cLuxJournal *apJournal, eLuxJournalState aState);

	void Reset();
	void OnEnter();
	void DestroySessionWidgets();
	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);

	tWidgetList mlstSessionWidgets;
	cWidgetDummy* mpRootWidget;
	float mfAlpha;

private:
	cLuxJournal *mpJournal;
	eLuxJournalState mState;
};

//----------------------------------------

class cLuxJournal_ListEntry
{
public:
	void AddWidget(iWidget *apWidget);
	void SetVisible(bool abX);

	tWidgetList mlstWidgets;
};

class cLuxJournal_ListPage
{
public:
	void SetVisible(bool abX);

	std::vector<cLuxJournal_ListEntry> mvEntries;
};

//----------------------------------------

class cLuxJournal : public iLuxUpdateable
{
friend class cLuxMusicHandler_SaveData;
friend class cLuxJournalStateData;
friend class cLuxJournal_SaveData;
public:	
	cLuxJournal();
	~cLuxJournal();
	
	void OnClearFonts();
	void LoadFonts();

	void OnStart();
	void Reset();

	void OnGameStart();

	void Update(float afTimeStep);

	void OnEnterContainer(const tString& asOldContainer);
	void OnLeaveContainer(const tString& asNewContainer);

	void OnDraw(float afFrameTime);

	cGuiSet* GetSet() { return mpGuiSet; }
	cViewport* GetViewport(){ return mpViewport; }

	void ExitPressed(bool abInstantExit);

	void Exit();

	void SetForceInstantExit(bool abX){ mbForceInstantExit = abX;}

	/**
	 * Coop: whose screen this journal is on. Set before switching container.
	 */
	void SetActivePlayer(cLuxPlayer *apPlayer){ mpActivePlayer = apPlayer; }
	cLuxPlayer* GetActivePlayer(){ return mpActivePlayer ? mpActivePlayer : gpBase->mpPlayer; }

	/**
	 * Coop: show on BOTH halves instead of just the opener's.
	 *
	 * For a note or diary picked up in the WORLD -- that is a thing that just
	 * happened to the party, so both players read it. Re-opening the same note
	 * later from the journal is one player's business and stays on their half.
	 * Cleared automatically when the journal closes.
	 */
	void SetShowOnBothPlayers(bool abX){ mbShowOnBothPlayers = abX; }
	bool GetShowOnBothPlayers(){ return mbShowOnBothPlayers; }

	/**
	 * Coop: both players are STOPPED for this one, not merely both shown it.
	 *
	 * ShowOnBothPlayers is about WHERE it draws. This is about whether the world
	 * keeps running underneath. They travel together for a note found in the
	 * world -- nobody should be fighting a Grunt while their partner reads --
	 * and apart for anything opened from the journal, which never stops anyone.
	 */
	void SetPauseBothPlayers(bool abX){ mbPauseBothPlayers = abX; }
	bool GetPauseBothPlayers(){ return mbPauseBothPlayers; }

	/**
	 * The level editor's "ReadByBoth" flag on the note or diary about to be
	 * picked up, handed over one pickup at a time.
	 *
	 * A latch rather than a parameter because the note is created deep inside
	 * cLuxItemType_Note::BeforeAddItem, three calls below the prop that knows the
	 * answer, and every one of those signatures is also used by the script
	 * AddNote() where there is no prop at all. Consumed on read, so a scripted
	 * note that follows a picked-up one cannot inherit its flag.
	 */
	/**
	 * What the last note or diary picked up in the WORLD decided, in words.
	 *
	 * Latched rather than read live because every flag involved is cleared the
	 * moment the journal closes -- so by the time anyone can look at a readout,
	 * the evidence is gone. This survives.
	 */
	void SetLastNoteDecision(const tString& asX){ msLastNoteDecision = asX; }
	const tString& GetLastNoteDecision(){ return msLastNoteDecision; }

	void SetNextNoteReadByBoth(bool abX){ mbNextNoteReadByBoth = abX; }
	bool ConsumeNextNoteReadByBoth(){ bool bX = mbNextNoteReadByBoth; mbNextNoteReadByBoth = false; return bX; }

	void SetOpenedFromInventory(bool abX){ mbOpenedFromInventory = abX;}

    cLuxNote* AddNote(const tString& asNameAndTextEntry, const tString& asImage);
	cLuxDiary* AddDiary(const tString& asNameAndTextEntry, const tString& asImage, int &alCurrentEntryIdx);
	
	bool AddQuestNote(const tString& asName, const tString& asNameAndTextEntry);
	bool DisableQuestNote(const tString& asName);
	cLuxQuestNote* GetQuestNote(const tString& asName);

	void ChangeState(eLuxJournalState aState);

	/**
	 * Coop: turn the page of an open note WITHOUT a pointer.
	 *
	 * There is one GUI cursor and, on a note both players are reading, two mice.
	 * Letting both push it means each of them drags it off whatever the other was
	 * about to click. So the pointer stays Player 1's and the second player turns
	 * the page from their own device through here instead -- which is also the
	 * only way a player on a pad has ever been able to.
	 *
	 * Past the last page it closes the note, because that is what turning the page
	 * means there, and closing is shared too.
	 */
	void CoopAdvanceNotePage();

	void OpenNote(cLuxNote *apNote, bool abNarration);
	cLuxNote* GetNote(int alIdx){ return mvNotes[alIdx];}

	void OpenDiary(cLuxDiary *apDiary, bool abNarration);
	void SetDiaryAsLastRead(cLuxDiary *apDiary);

	void OpenLastReadText();
	
private:
	cLuxDiaryContainer* CreateDiaryContainer(const tString& asType);

	void SetupLabel(cWidgetLabel *apLabel, const cVector2f& avSize, int alIdx, eLuxJournalState aState, iFontData *apFont=NULL, eFontAlign aFontAlign=eFontAlign_Center);
	void SetupImage(cWidgetImage *apImage, int alIdx, eLuxJournalState aState);
	void SetupNavigationWidgets(eLuxJournalState aState, int alListIndex, int alForwardIndex, int alBackwardIndex, cWidgetDummy *apRoot);

	void AddSessionWidget(eLuxJournalState aState, iWidget *apWidget);
	void ResetSessionVars();

	void SetStateBackgroundGfx(const tString& asFile);

	void LoadText(const tWString &asName ,const tWString &asText);
	void LoadNarrationText(const tWString &asName ,const tWString &asText);
	void SetNotePage(int alPageNum);
	
	int GetNoteListIndex(eLuxJournalState aState);//Return values: 0=notes, 1=diaries, 2=quests
	void SetNoteListPage(int alPageNum, eLuxJournalState aState);

	void CreateGui();
	void DestroyGui();

	void CreateMainGui();
	void CreateNotesGui();
	void CreateDiariesGui();
	void CreateQuestNotesGui();
	void CreateOpenNoteGui();

	void CreateBackground();
	void RenderBackgroundImage();
	void CreateScreenTextures();
	void DestroyBackground();

	///////////////////////
	// Gui callbacks
	bool MainMenuTextClick(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(MainMenuTextClick);

	bool MainMenuUIButtonPress(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(MainMenuUIButtonPress);

	bool MainMenuTextOnDraw(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(MainMenuTextOnDraw);
	
	bool ListTextOnDraw(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(ListTextOnDraw);

	bool NoteTextClick(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(NoteTextClick);

	bool NoteBackClick(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(NoteBackClick);

	bool UIListenerJournalPress(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(UIListenerJournalPress);

	bool DiaryTextClick(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(DiaryTextClick);
	
	bool ImageButtonOnDraw(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(ImageButtonOnDraw);

	bool NoteArrowClick(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(NoteArrowClick);

	bool NoteClickFrameClick(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(NoteClickFrameClick);

	bool JournalItemUIButtonPress(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(JournalItemUIButtonPress);

	bool UIListenerArrowPress(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(UIListenerArrowPress);

	bool UIListenerButtonPress(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(UIListenerButtonPress);

	bool NoteBackUIButtonPress(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(NoteBackUIButtonPress);

	//////////////
	// Variables
	bool mbActive;
	float mfAlpha;

	bool mbForceInstantExit;
	bool mbOpenedFromInventory;

	int mlLastReadTextCat; //Only used by diary
	int mlLastReadTextEntry;
	int mlLastReadTextType;	//0=note 1=diary

	std::vector<cLuxNote*> mvNotes;
	std::vector<cLuxDiaryContainer*> mvDiaryContainers;
	std::vector<cLuxQuestNote*> mvQuestNotes;
	
	cGuiGfxElement *mpStateBackgroundGfx;

	float mfMouseOverPulse;

	tWString msHeader;
	std::vector<cLuxNotePage> mvPages;
	int mlCurrentNotePage;
	
	cSoundEntry *mpVoiceEntry;
	int mlVoiceEntryID;

	std::vector<cLuxJournal_ListPage> mvNoteListPages[3];//0=notes, 1=diaries, 2=quests
	int mlCurrentNoteListPage[3];

	//////////////
	// Data
	cGui *mpGui;
	cScene *mpScene;
	cGraphics *mpGraphics;

	cViewport *mpViewport;
	cLuxPlayer *mpActivePlayer;
	bool mbShowOnBothPlayers;
	bool mbPauseBothPlayers;
	bool mbNextNoteReadByBoth;
	tString msLastNoteDecision;
	cGuiSkin *mpGuiSkin;
	cGuiSet *mpGuiSet;

	eLuxJournalState mCurrentState;
	std::vector<cLuxJournalStateData*> mvStateData;

	tLuxJournalWidgetData mlstSessionWidgetData;

	iWidget		 *mpWidgetDefaultNav[eLuxJournalState_LastEnum];
	cWidgetImage *mpImageForward[eLuxJournalState_LastEnum];
	iWidget		 *mpWidgetReturn[eLuxJournalState_LastEnum];
	cWidgetImage *mpImageBackward[eLuxJournalState_LastEnum];
	
	iTexture *mpScreenTexture;
	cGuiGfxElement *mpScreenGfx;
	iTexture *mpScreenBgTexture;
	cGuiGfxElement *mpScreenBgGfx;

	cGuiGfxElement *mpWhiteGfx;

	iFontData *mpFontDefault;
	iFontData *mpFontMenu;

	iGpuProgram *mpEffectProgram;

	cVector2f mvScreenSize;
	cVector2f mvGuiSetCenterSize;//Size of the part that is inside a 4:3 ratio!
	cVector2f mvGuiSetSize;
	cVector2f mvGuiSetOffset;
	cVector3f mvGuiSetStartPos;

	float mfNoteTextWidth;
	int mlNoteMaxPageRows;
	cVector2f mvNoteFontSize;
	float mfNoteRowDist;
	cVector2f mfNoteHeaderFontSize;
	float mfNoteHeaderStartY;
	float mfNoteTextStartY;

	float mfNoteListHeaderY;
	float mfMaxNoteListY;

	float mfBackTextY;
	cVector2f mvBackTextFontSize;

};

//----------------------------------------------


#endif // LUX_JOURNAL_H
