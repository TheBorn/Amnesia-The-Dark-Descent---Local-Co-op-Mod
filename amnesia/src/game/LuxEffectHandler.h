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

#ifndef LUX_EFFECT_HANDLER_H
#define LUX_EFFECT_HANDLER_H

//----------------------------------------------

#include "LuxBase.h"

//----------------------------------------------


class iLuxEffect
{
public:
	iLuxEffect() : mbActive(false) {}
	~iLuxEffect(){}

	virtual void Update(float afTimeStep)=0;
	virtual void OnDraw(float afFrameTime)=0;
	virtual void Reset()=0;

	virtual void DoAction(eLuxPlayerAction aAction, bool abPressed){}


	bool IsActive(){ return mbActive;}
	void SetActive(bool abX){ mbActive = abX;}

protected:
	bool mbActive;    
};


//----------------------------------------------

class cLuxEffect_PlayCommentary : public iLuxEffect
{
public:
	cLuxEffect_PlayCommentary();
	~cLuxEffect_PlayCommentary();

	void Start(const tString &asTalker,const tString &asTopic, const tString &asFile, int alIconId);
	void Stop();
	
	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);
	void Reset();

private:
	//////////////////////
	//Data
	cSoundHandler *mpSoundHandler;
	cMusicHandler *mpMusicHandler;
	
	//////////////////////
	//Variables
	cSoundEntry *mpSoundEntry;
	int mlSoundEntryID;

	tString msTalker;
	tString msTopic;
	int mlIconID;
};

//----------------------------------------------

class cLuxEffect_EmotionFlash : public iLuxEffect
{
public:
	cLuxEffect_EmotionFlash();
	~cLuxEffect_EmotionFlash();

	void ClearFonts();
	void LoadFonts();

	void Start(const tString &asTextCat, const tString &asTextEntry, const tString &asSound);
	void Reset();
	
	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);
	
	void DoAction(eLuxPlayerAction aAction, bool abPressed);

private:
	/**
	 * Coop: the player who triggered this vision. Start() runs inside their
	 * callback, but Update() and OnDraw() run from the module loop where
	 * gpBase->mpPlayer is always P1 -- so the actor has to be remembered, not
	 * re-read. Compared against the live players, never trusted: P2 can be
	 * destroyed mid-vision.
	 */
	cLuxPlayer* GetActingPlayer();
	void DrawVision(cGuiSet *apSet);

	cLuxPlayer *mpActingPlayer;

	cGuiGfxElement *mpWhiteGfx;
	iFontData *mpFont;
	cVector2f mvFontSize;

	float mfAlpha;

	int mlStep;
	float mfCount;
	
	float mfTextAlpha;
	float mfTextTime;
	tWStringVec mvTextRows;
};

//----------------------------------------------

class cLuxEffect_RadialBlur : public iLuxEffect
{
	friend class cLuxEffectHandler_SaveData;
public:
	cLuxEffect_RadialBlur();

	void SetBlurStartDist(float afDist);
	void FadeTo(float afSize, float afSpeed);

	float GetCurrentSize(){ return mfSize;}

	void Update(float afTimeStep);
	void OnDraw(float afFrameTime){}
	void Reset();

private:
	float mfSize;
	float mfSizeGoal;
	float mfFadeSpeed;
	float mfBlurStartDist;
};

//----------------------------------------------

class cLuxEffect_SepiaColor : public iLuxEffect
{
	friend class cLuxEffectHandler_SaveData;
public:
	cLuxEffect_SepiaColor();

	void FadeTo(float afAmount, float afSpeed);

	void Update(float afTimeStep);
	void OnDraw(float afFrameTime){}
	void Reset();

	float GetAmount(){ return mfAmount; }

	/**
	 * Coop: draw the sepia wash straight onto a gui set.
	 *
	 * The real sepia is a post-effect colour LUT, and post effects are dead in
	 * split-screen (the composite lives on the main viewport, which coop switches
	 * off). Driving per-viewport composites corrupted the picture, so the tint is
	 * done here instead, on the same gui-set path the flash, subtitles and the
	 * transition fade already use successfully.
	 *
	 * NOT driven from OnDraw(): this effect calls SetActive(false) as soon as the
	 * fade reaches its goal, so the effect handler stops dispatching to it while
	 * the flashback is still running and holding the tint at full. cLuxPlayer::
	 * OnDraw calls this once per player per frame instead.
	 */
	void DrawCoopOverlay(cGuiSet *apSet);

private:
	cGuiGfxElement *mpSepiaGfx;

	float mfAmount;
	float mfAmountGoal;
	float mfFadeSpeed;
};

//-----------------------------------------

class cLuxEffect_ShakeScreen_Shake
{
public:
	float mfMaxSize;
	float mfSize;
	float mfTime;
	float mfFadeInTime;
	float mfMaxFadeInTime;
	float mfFadeOutTime;
	float mfMaxFadeOutTime;

	//Who feels this one. -1 both, 0 P1, 1 P2. Overlapping shakes can each have a
	//different target, so the two cameras are computed independently.
	int mlCoopTargetPlayer;
};

class cLuxEffect_ShakeScreen : public iLuxEffect
{
public:
	cLuxEffect_ShakeScreen();
	~cLuxEffect_ShakeScreen();

	//alCoopTargetPlayer: -1 both, 0 P1, 1 P2. Defaulted, so every existing caller
	//keeps shaking both.
	void Start(float afAmount, float afTime, float afFadeInTime,float afFadeOutTime,
			   int alCoopTargetPlayer = -1);

	void Update(float afTimeStep);
	void OnDraw(float afFrameTime){}
	void Reset();

private:

	std::list<cLuxEffect_ShakeScreen_Shake> mlstShakes;
};

//----------------------------------------------

class cLuxEffect_ImageTrail : public iLuxEffect
{
friend class cLuxEffectHandler_SaveData;
public:
	cLuxEffect_ImageTrail();

	void FadeTo(float afAmount, float afSpeed);

	void Update(float afTimeStep);
	void OnDraw(float afFrameTime){}
	void Reset();

private:
	float mfAmount;
	float mfAmountGoal;
	float mfFadeSpeed;
};

//----------------------------------------------

class cLuxEffect_Fade : public iLuxEffect
{
friend class cLuxEffectHandler_SaveData;
public:
	cLuxEffect_Fade();
	~cLuxEffect_Fade();

	void FadeIn(float afTime);
	void FadeOut(float afTime);

	/**
	 * Coop: a fade belongs to whoever asked for it -- a cutscene P2 set off
	 * should not black out P1's half of the screen while they are still
	 * playing. Level transitions are the exception: the whole map is going
	 * away, so both screens fade together. Use these there.
	 */
	void FadeInGlobal(float afTime);
	void FadeOutGlobal(float afTime);

	bool IsFading();

	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);
	void Reset();

	void SetDirectAlpha(float afX);

private:
	// Coop: which half/halves this fade covers. Defaults to both, because a
	// fade restored from a save or left over from a transition must never
	// leave one player staring at the world while the other is black.
	void SetCoopTarget(bool abGlobal);

	/** Drive the halves SetCoopTarget just selected toward afGoal over afTime. */
	void ApplyFade(float afGoal, float afTime);

	bool mbShowOnP1;
	bool mbShowOnP2;

	cGuiGfxElement *mpWhiteGfx;

	///////////////////////////////////////////////////////////////////////////
	// ONE SET PER PLAYER. Index 0 = Player 1, 1 = Player 2.
	//
	// A single shared alpha was THE split-screen blackout. FadeIn and FadeOut both
	// call SetCoopTarget, which re-decides which half the quad covers from whoever
	// the acting player is -- but neither touched the alpha. So a fade-out
	// attributed to one player (that half goes black, alpha ~1) followed by the
	// matching fade-in attributed to the OTHER player moved the still-black quad
	// onto the second player's half: their screen went instantly, completely black
	// and then faded back in on its own, while the first player's cleared the same
	// frame. Trigger volumes hand the enter and leave edges to different players
	// routinely, so it fires during ordinary play with no pattern.
	//
	// Per player, the two fades simply cannot interfere.
	float mfGoalAlpha[2];
	float mfAlpha[2];
	float mfFadeSpeed[2];
};


//----------------------------------------------

class cLuxPlayer;

class cLuxEffect_SanityGainFlash : public iLuxEffect
{
public:
	cLuxEffect_SanityGainFlash();
	~cLuxEffect_SanityGainFlash();

	void Start();

	/**
	 * Coop: flash only the screen of the player who actually gained. Start()
	 * on its own has no player context and so flashes both.
	 */
	void StartForPlayer(cLuxPlayer *apPlayer);

	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);
	void Reset();

	void DrawFlash(cGuiSet *apSet ,float afTimeStep);

private:
	cGuiGfxElement *mpWhiteGfx;

	float mfAlpha;

	// Coop: which screen(s) this flash belongs to. One shared timeline is
	// enough -- a reward given to both players is given in the same frame --
	// but the halves must flash independently when only one player earned it.
	bool mbShowOnP1;
	bool mbShowOnP2;

	int mlStep;
	float mfCount;

	float mfFadeInSpeed;
	float mfWhiteSpeed;
	float mfFadeOutSpeed;

	cColor mColor;
	tString msSound;
	float mfFadeInTime;
	float mfFadeOutTime;
};

//----------------------------------------------

class cLuxEffect_Flash : public iLuxEffect
{
public:
	cLuxEffect_Flash();
	~cLuxEffect_Flash();
	
	void Start(float afFadeIn, float afWhite, float afFadeOut);

	/**
	 * Coop: flash ONLY the screen this flash belongs to.
	 *
	 * Start() has no player context, so it flashes both halves -- correct for a
	 * scripted story beat (FadeInFlash, flashback start/end), where both players
	 * are living through the same moment.
	 *
	 * Wrong for the respawn flash. That one belongs to the death sequence, and
	 * the rest of that sequence (the fade, the hint text) is drawn only on the
	 * screen running it -- so the other player got a white flash with nothing
	 * before it to explain what had happened.
	 */
	void StartForPlayer(cLuxPlayer *apPlayer, float afFadeIn, float afWhite, float afFadeOut);

	void Reset();

	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);

private:
	void StartTimeline(float afFadeIn, float afWhite, float afFadeOut);

	cGuiGfxElement *mpWhiteGfx;

	float mfAlpha;

	// Coop: which half/halves this flash draws on. Both by default -- a flash
	// with no owner is a shared story beat. See StartForPlayer.
	bool mbShowOnP1;
	bool mbShowOnP2;

	int mlStep;
	float mfCount;

	float mfFadeInSpeed;
	float mfWhiteSpeed;
	float mfFadeOutSpeed;
};

//----------------------------------------------

typedef std::list<cLuxVoiceData> tLuxVoiceDataList;
typedef tLuxVoiceDataList::iterator tLuxVoiceDataListIt;

class cLuxEffect_PlayVoice : public iLuxEffect
{
friend class cLuxEffectHandler_SaveData;
public:
	cLuxEffect_PlayVoice();
	~cLuxEffect_PlayVoice();

	void StopVoices(float afFadeOutSpeed);
	void AddVoice(	const tString& asVoiceFile, const tString& asEffectFile,
					const tString& asTextCat, const tString& asTextEntry, bool abUsePostion, 
					const cVector3f& avPosition, float afMinDistance, float afMaxDistance);

	void PauseCurrentVoices();
	void UnpauseCurrentVoices();

	void SetOverCallback(const tString& asFunc){ msOverCallback = asFunc;}

	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);
	void Reset();

	void SetVolumeMul(float afMul);
	float GetVolumeMul(float afMul){ return mfVolumeMul;}

	/**
	 * Returns true if all voices (not effect files) are done playing
	 */
	bool VoiceDonePlaying();

private:
	//////////////////////
	//Data
	cSoundHandler *mpSoundHandler;
	cVector2f mvFontSize;
	float mfRowWidth;

	//////////////////////
	//Variables
	bool mbPaused;
	tString msOverCallback;
	tLuxVoiceDataList mlstVoices;
	cSoundEntry *mpVoiceEntry;
	int mlVoiceEntryID;
	cSoundEntry *mpEffectEntry;
	int mlEffectEntryID;
	std::vector<tWString> mvCurrentTextRows;

	float mfVolumeMul;
};

//----------------------------------------------

class cLuxEffectHandler : public iLuxUpdateable
{
public:	
	cLuxEffectHandler();
	~cLuxEffectHandler();

	///////////////////////////
	// General
	void OnClearFonts();
	void LoadFonts();
	void OnStart();
	void Update(float afTimeStep);
	void Reset();

	void OnMapEnter(cLuxMap *apMap);
	void OnMapLeave(cLuxMap *apMap);

	void OnDraw(float afFrameTime);

	void DoAction(eLuxPlayerAction aAction, bool abPressed);
	
	///////////////////////////
	// Properties
	bool GetPlayerIsPaused(){ return mbPlayerIsPaused;}
	void SetPlayerIsPaused(bool abX);
	/**
	 * Coop: freeze only apPlayer. Un-freezing still reaches BOTH, so a sequence
	 * one player starts and the other ends can never leave someone stuck.
	 */
	void SetPlayerIsPausedFor(cLuxPlayer *apPlayer, bool abX);

	///////////////////////////
	// Effects
	cLuxEffect_Fade *GetFade(){ return mpFade;}
	cLuxEffect_Flash *GetFlash(){ return mpFlash;}
	cLuxEffect_SanityGainFlash *GetSanityGainFlash(){ return mpSanityGainFlash;}
	cLuxEffect_PlayVoice *GetPlayVoice(){ return mpPlayVoice;}
	cLuxEffect_ImageTrail *GetImageTrail(){ return mpImageTrail;}
	cLuxEffect_ShakeScreen *GetScreenShake(){ return mpScreenShake;}
	cLuxEffect_SepiaColor *GetSepiaColor(){ return mpSepiaColor;}
	cLuxEffect_RadialBlur *GetRadialBlur(){ return mpRadialBlur;}
	cLuxEffect_EmotionFlash *GetEmotionFlash(){ return mpEmotionFlash;}
	cLuxEffect_PlayCommentary *GetPlayCommentary(){ return  mpPlayCommentary;}

private:
	cLuxEffect_Fade *mpFade;
	cLuxEffect_Flash *mpFlash;
	cLuxEffect_SanityGainFlash *mpSanityGainFlash;
	cLuxEffect_PlayVoice *mpPlayVoice;
	cLuxEffect_ImageTrail *mpImageTrail;
	cLuxEffect_ShakeScreen *mpScreenShake;
	cLuxEffect_SepiaColor *mpSepiaColor;
	cLuxEffect_RadialBlur *mpRadialBlur;
	cLuxEffect_EmotionFlash *mpEmotionFlash;
	cLuxEffect_PlayCommentary *mpPlayCommentary;

	std::vector<iLuxEffect*> mvEffects;	

	bool mbPlayerIsPaused;
};

//----------------------------------------------


#endif // LUX_EFFECT_HANDLER_H
