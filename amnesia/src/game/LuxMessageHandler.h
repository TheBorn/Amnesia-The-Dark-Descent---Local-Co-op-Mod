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

#ifndef LUX_MESSAGE_HANDLER_H
#define LUX_MESSAGE_HANDLER_H

//----------------------------------------------

#include "LuxBase.h"

//----------------------------------------


class cLuxMessageHandler : public iLuxUpdateable
{
friend class cLuxMusicHandler_SaveData;
public:	
	cLuxMessageHandler();
	~cLuxMessageHandler();
	
	void LoadFonts();
	void OnStart();
	void Update(float afTimeStep);
	void Reset();

	void LoadUserConfig();
	void SaveUserConfig();

	void OnMapEnter(cLuxMap *apMap);
	void OnMapLeave(cLuxMap *apMap);

	void StarQuestAddedMessage();

	void StartPauseMessage(const tWString& asText, bool abYesNo, iLuxMessageCallback *apCallback);
	
	/**
	* if time is <=0 then the life time is calculated based on string length.
	*/
	void SetMessage(const tWString& asText, float afTime);
	void SetMessage(const tWString& asText, float afTime, int alPlayerIndex);

	/**
	 * The same, but for something the PAIR are being told rather than one of
	 * them: an item picked up out of the world, an examine area, a line the
	 * story itself put on screen.
	 *
	 * In FORCED co-op that draws on both halves. A story written for co-op
	 * decides for itself who is told what, and single player has only one
	 * screen, so in both of those this is exactly SetMessage.
	 *
	 * Deliberately NOT what the ordinary SetMessage does. The messages that
	 * stay with one player are the ones that answer something they just tried
	 * -- a locked door, an empty barrel, a bag with no room, an item that does
	 * not work here. Those are feedback on their own action, and putting them
	 * on the other player's screen is telling them about a failure they had
	 * nothing to do with.
	 */
	void SetMessageForBoth(const tWString& asText, float afTime);
	bool IsMessageActive(){ return mfMessageTime>0; }

	void OnDraw(float afFrameTime);

	/** True when a message the pair share should be drawn on both halves. */
	bool CoopMessageGoesToBoth();

	void DoAction(eLuxPlayerAction aAction, bool abPressed);

	bool IsPauseMessageActive(){ return mbPauseMessageActive; }
	void SetPauseMessageActive(bool abX);

	bool ShowSubtitles(){ return mbShowSubtitles;}
	void SetShowSubtitles(bool abX){ mbShowSubtitles=abX;}

	bool ShowEffectSubtitles(){ return mbShowEffectSubtitles; }
	void SetShowEffectSubtitles(bool abX){ mbShowEffectSubtitles=abX; }
private:
	void DrawQuestAdded();
	void DrawMessage();
	void DrawPauseMessage();
	
	
	//////////////////
	// Data
	cGuiGfxElement *mpBlackGfx;
	iFontData *mpFont;

	cGuiGfxElement *mpQuestAddedIcon;
	tString msQuestAddedSound;
	
	cVector2f mvFontSize;
	
	//////////////////
	// Variables
	bool mbShowSubtitles;
	bool mbShowEffectSubtitles;

	bool mbPauseMessageActive;
	float mfPauseMessageAlpha;

	float mfMessageAlpha;
	float mfMessageTime;
	int mlMessagePlayerIndex;  // 0 = P1, 1 = P2

	//Set by SetMessageForBoth, cleared by every other path into SetMessage, so a
	//shared line cannot outlive itself and put the next private one on both
	//screens.
	bool mbMessageOnBothPlayers;

	bool mbQuestMessageActive;
	float mfQuestMessageAlpha;
	float mfQuestMessageTime;

	cLinearOscillation mQuestOscill;

	tWStringVec mvMessageRows;

	tWStringVec mvLines;
	bool mbMessageYesNo;
	iLuxMessageCallback* mpCallback;
};

//----------------------------------------------


#endif // LUX_MESSAGE_HANDLER_H
