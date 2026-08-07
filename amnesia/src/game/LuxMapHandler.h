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

#ifndef LUX_MAP_HANDLER_H
#define LUX_MAP_HANDLER_H

//----------------------------------------------

#include "LuxBase.h"

//----------------------------------------------

class cLuxMap;
class cLuxSavedGameMapCollection;
class cLuxModelCache;
class cLuxPostEffect_Insanity;

typedef std::list<cLuxMap*> tLuxMapList;
typedef tLuxMapList::iterator tLuxMapListIt;

//----------------------------------------------

class cMapHandlerSoundCallback : public iSoundEntityGlobalCallback
{
public:
	cMapHandlerSoundCallback();

	void OnStart(cSoundEntity *apSoundEntity);

private:
	tStringVec mvEnemyHearableSounds;
};

//----------------------------------------------

class cLuxDebugRenderCallback : public iRendererCallback
{
public:
	cLuxDebugRenderCallback();

	void OnPostSolidDraw(cRendererCallbackFunctions* apFunctions);

	void OnPostTranslucentDraw(cRendererCallbackFunctions* apFunctions);

	iPhysicsWorld* mpPhysicsWorld;
	iLowLevelGraphics* mpLowLevelGfx;
};

//----------------------------------------------

class cLuxCoopRenderCallback : public iRendererCallback
{
public:
	void OnPostSolidDraw(cRendererCallbackFunctions* apFunctions);
	void OnPostTranslucentDraw(cRendererCallbackFunctions* apFunctions);
};

//----------------------------------------------

/**
 * Per-viewport avatar visibility: right before a viewport renders its world,
 * show every coop avatar EXCEPT the one belonging to the camera that is
 * rendering — a player never sees their own avatar. Because this runs before
 * the renderer, the state also applies to that view's water reflections and
 * shadow maps.
 */
class cLuxCoopAvatarVisCallback : public iViewportCallback
{
public:
	cLuxCoopAvatarVisCallback() : mpViewport(NULL) {}

	void OnPreWorldDraw();
	void OnPostWorldDraw(){}

	cViewport *mpViewport;
};


//----------------------------------------------

class cLuxMapHandler_ChangeMap
{
public:
	cLuxMapHandler_ChangeMap() : mbActive(false){}

	bool mbActive;
	tString msMapFile;
	tString msStartPos;
	tString msSound;
};

//----------------------------------------------

class cLuxMapHandler : public iLuxUpdateable
{
friend class cMapHandlerSoundCallback;
public:	
	cLuxMapHandler();
	~cLuxMapHandler();
	
	void OnStart();
	void Update(float afTimeStep);
	void Reset();
    void OnQuit();

	void LoadUserConfig();
	void SaveUserConfig();

	void CreateDataCache();
	void DestroyDataCache();

	void UpdateViewportRenderProperties();

	void SetUpdateActive(bool abX);
	
	void RenderSolid(cRendererCallbackFunctions* apFunctions);

	void OnEnterContainer(const tString& asOldContainer);
	void OnLeaveContainer(const tString& asNewContainer);

	void ChangeMap(const tString& asMapName, const tString& asStartPos, const tString& asStartSound, const tString& asEndSound);

	bool MapIsLoaded(){ return mpCurrentMap != NULL;}

	cLuxMap* LoadMap(const tString& asName, bool abLoadEntities);
	void DestroyMap(cLuxMap* apMap, bool abRunScript);


	void SetCurrentMap(cLuxMap* apMap, bool abRunScript, bool abFirstTime, const tString& asPlayerPos);
	cLuxMap* GetCurrentMap(){ return mpCurrentMap;}

	cViewport* GetViewport(){ return mpViewport;}

	////////////////////
	// Co-op split-screen
	enum eSplitScreenMode
	{
		eSplitScreenMode_LeftRight = 1,
		eSplitScreenMode_TopBottom = 2,
		eSplitScreenMode_DualMonitor = 3
	};

	void SetCoopMode(bool abX);
	bool GetCoopMode(){ return mbCoopActive; }

	/**
	 * Whether the STORY was written knowing there are two players, declared from
	 * SupportsCoop="true" in its custom_story_settings.cfg.
	 *
	 * This is not "is co-op running" -- that is GetCoopMode. It is the difference
	 * between a story designed for two and the main game being played co-op
	 * anyway, and it decides how the unsuffixed script functions read an
	 * instruction that does not name a player. A story that never says a word
	 * about co-op keeps every behaviour it has today.
	 *
	 * Saved with the rest of the co-op configuration, because the global script's
	 * OnGameStart does not run again when a save is loaded.
	 */
	void SetCoopStorySupport(bool abX){ mbCoopStorySupport = abX; }
	bool GetCoopStorySupport(){ return mbCoopStorySupport; }

	//////////////////////////////////
	// Coop Options -- story-scriptable gameplay options (the SetCoopAllow* /
	// SetCoopGlobal* script functions). Owned here so they live and die with
	// the rest of the coop state and ride the same save plumbing as
	// mbCoopStorySupport.
	//
	// In a NATIVE coop story (GetCoopStorySupport()) these are THE effective
	// values, live-editable from the debug menu's Coop Options section. Under
	// forced coop the debug menu's own toggles rule instead; these stay
	// settable so a story may configure before declaring support.
	void SetCoopOptCrouchBoost(bool abX){ mbCoopOptCrouchBoost = abX; }
	bool GetCoopOptCrouchBoost(){ return mbCoopOptCrouchBoost; }
	void SetCoopOptPlayerCollision(bool abX){ mbCoopOptPlayerCollision = abX; }
	bool GetCoopOptPlayerCollision(){ return mbCoopOptPlayerCollision; }
	void SetCoopOptMonsterKilling(bool abX){ mbCoopOptMonsterKilling = abX; }
	bool GetCoopOptMonsterKilling(){ return mbCoopOptMonsterKilling; }
	void SetCoopOptMonsterDamageMul(float afX){ mfCoopOptMonsterDamageMul = afX; }
	float GetCoopOptMonsterDamageMul(){ return mfCoopOptMonsterDamageMul; }
	void SetCoopOptGlobalLantern(bool abX){ mbCoopOptGlobalLantern = abX; }
	bool GetCoopOptGlobalLantern(){ return mbCoopOptGlobalLantern; }

	/**
	 * Effective player-vs-player collision options: a native coop story uses
	 * the scripted values above, forced coop uses the debug menu's toggles.
	 * Consumed by the blocker system in Update().
	 */
	bool CoopPlayerCollisionEffective();
	bool CoopCrouchBoostEffective();

	// True when the split viewports really are running the post-effect pass, so the
	// hud sepia fallback knows to stay out of the way and not double up.
	bool GetCoopPostEffectsActive();

	/**
	 * The insanity distortion instance THIS player's low sanity should drive.
	 *
	 * The shared instance lives on the MAIN viewport's composite, which
	 * split-screen hides -- so in coop nobody saw the sanity wave at all, and
	 * both players' sanity helpers overwrote one another's values on the one
	 * instance. In split coop with post effects, each player gets the instance
	 * on their own composite; in single player P1 keeps the main one; a P2
	 * with no composite of their own gets NULL (drive nothing, fight nobody).
	 */
	cLuxPostEffect_Insanity* GetInsanityEffectForPlayer(cLuxPlayer *apPlayer);

	// Mirrors the Options menu's insanity-effect toggle onto the per-player
	// coop instances (the menu itself only reaches the main one).
	void SetCoopInsanityDisabled(bool abX);

	// Positions P2's character body/camera next to P1 (used on spawn, map
	// transitions, checkpoint respawns and the debug teleports).
	void PlacePlayer2NearPlayer1();

	// Debug: teleport one player to the other's position (offset a step to
	// the side). Used by the Player Teleport debug option (F1 / F2 / sticks).
	void TeleportPlayerToOther(cLuxPlayer *apMover, cLuxPlayer *apTarget);
	cViewport* GetCoopViewport(){ return mpCoopViewport; }
	cViewport* GetCoopP1Viewport(){ return mpCoopP1Viewport; }
	cGuiSet* GetCoopHudSet(){ return mpCoopHudSet; }

	void SetSplitScreenMode(int alMode);
	int GetSplitScreenMode(){ return mlSplitScreenMode; }

	void SetDualMonitorIndex(int alIdx);
	int GetDualMonitorIndex(){ return mlDualMonitorIndex; }

	// Background capture for inventory/journal/pause.
	// The map handler is the single owner of viewport state during a capture;
	// GetBackgroundCaptureMode decides what the snapshot should contain.
	enum eBackgroundCaptureMode
	{
		eBackgroundCaptureMode_None = 0,	// no coop: plain single-view capture
		eBackgroundCaptureMode_Composite,	// full-screen menu in split-screen: both views + letterbox bars
		eBackgroundCaptureMode_P1Full,		// P1 inventory: P1's view alone, rendered full-screen
		eBackgroundCaptureMode_P2Full,		// P2 inventory: P2's view alone, rendered full-screen
		eBackgroundCaptureMode_DualP1		// full-screen menu in dual monitor: P1's screen only
	};
	eBackgroundCaptureMode GetBackgroundCaptureMode();

	void PrepareBackgroundCapture();
	void RestoreBackgroundCapture();

	// Dual-monitor: mirror a full-screen menu's GUI set (journal/notes, pause
	// menu) onto P2's monitor so both players see it. No-ops outside
	// dual-monitor coop. The mirror viewport is GUI-only and lazily created;
	// Add re-positions it over P2's monitor and shows it, Remove hides it.
	void AddDualMenuMirror(cGuiSet *apSet);
	void RemoveDualMenuMirror(cGuiSet *apSet);

	// Player 2
	void CreatePlayer2(cLuxMap* apMap);
	void DestroyPlayer2(cLuxMap* apMap);
	void UpdatePlayer2(float afTimeStep);
	iCharacterBody* GetPlayer2Body(){ return mpCoopCharBody; }
	cCamera* GetPlayer2Camera(){ return mpCoopCamera; }

	const tString& GetMapFolder(){ return msMapFolder;}
	void SetMapFolder(const tString& asFolder){ msMapFolder = asFolder;}

	void PauseSoundsAndMusic();
	void ResumeSoundsAndMusic();

	iPostEffect *GetPostEffect_Bloom(){ return mpPostEffect_Bloom;}
	iPostEffect *GetPostEffect_ImageTrail(){ return mpPostEffect_ImageTrail;}
	iPostEffect *GetPostEffect_Sepia(){ return mpPostEffect_Sepia;}
	iPostEffect *GetPostEffect_RadialBlur(){ return mpPostEffect_RadialBlur;}

	void ClearSaveMapCollection();
	cLuxSavedGameMapCollection *GetSavedMapCollection(){ return mpSavedGame;}
	void SetSavedMapCollection(cLuxSavedGameMapCollection *apMaps);

	tString FileToMapName(const tString& asFile);

	void SetShowCommentary(bool abX);
	bool GetShowCommentary(){ return mbShowCommentary;}

	void AppLostInputFocus();
	void AppGotInputFocus();

	//////////////////////////////////
	// Used to lock the SavedMapCollection
    iMutex *mpSavedGameMutex; 
private:
	void LoadMainConfig();
	void SaveMainConfig();

	void CheckMapChange(float afTimeStep);

	void GetSplitViewportRects(int aiScreenW, int aiScreenH,
		cVector2l &avP1Pos, cVector2l &avP1Size,
		cVector2l &avP2Pos, cVector2l &avP2Size);

	/**
	 * Ask for the spanning window to be on or off. Records the wish only.
	 *
	 * Reconfiguring the window is not free and not invisible: SDL_SetWindowSize,
	 * SetWindowPosition, SetWindowBordered and SetWindowFullscreen each tear down
	 * and rebuild the swap chain, and when the window spans two displays the
	 * monitors renegotiate the link -- seconds of flickering, then black, then
	 * flickering again while they re-sync.
	 *
	 * A map load calls Reset() and then the coop setup back to back, so this used
	 * to fire OFF and straight back ON inside a single frame: two full window
	 * reconfigurations for no net change, twice the disruption of either one, and
	 * the log caught bursts of ten in one session. Recording the wish and settling
	 * it once per frame collapses that pair to nothing at all.
	 */
	void ApplyDualMonitorWindow(bool abEnable);

	/** Settle the window against what was asked for. Once per frame, no more. */
	void UpdateDualMonitorWindow();

	/** Does the actual reconfiguring. Only ever called from UpdateDualMonitorWindow. */
	void ApplyDualMonitorWindowNow(bool abEnable);

	cLuxDebugRenderCallback mRenderCallback;
	cLuxCoopRenderCallback mCoopRenderCallback;

	// Per-viewport avatar visibility (main, coop P1, coop P2)
	cLuxCoopAvatarVisCallback mAvatarVisCallbackMain;
	cLuxCoopAvatarVisCallback mAvatarVisCallbackP1;
	cLuxCoopAvatarVisCallback mAvatarVisCallbackP2;

	tString msMapFolder;

	cLuxModelCache *mpDataCache;

	cLuxMap* mpCurrentMap;

	tLuxMapList mlstMaps;

	cViewport *mpViewport;

	// Co-op
	cViewport *mpCoopViewport;		// P2's viewport
	cViewport *mpCoopP1Viewport;	// P1's dedicated coop viewport
	cGuiSet *mpCoopHudSet;			// P2's HUD GUI set
	cGuiGfxElement *mpCoopDarkOverlayGfx;	// Dark overlay for P2 during menus
	bool mbCoopActive;
	bool mbCoopStorySupport;
	int mlSplitScreenMode;			// 1=left/right, 2=top/bottom, 3=dual monitor
	int mlDualMonitorIndex;			// SDL display index for P2's monitor

	// Dual monitor window state (saved when entering dual monitor mode)
	bool mbDualMonitorWindowActive;	//what the window actually IS
	bool mbDualMonitorWanted;		//what it has been asked to be

	//Last state SetupCoopViewportPostEffects actually applied, so the per-tick
	//re-assert can early-out instead of reassigning composites six times a frame.
	int  mlLastAppliedPostEffectMode;	//-1 = nothing applied yet
	bool mbLastAppliedPostEffectOn;
	int mlSavedWindowX;
	int mlSavedWindowY;
	int mlSavedWindowW;
	int mlSavedWindowH;
	unsigned int mlSavedWindowFlags;  // SDL window flags (fullscreen, borderless, etc)

	// Player 2
	cCamera *mpCoopCamera;
	iCharacterBody *mpCoopCharBody;
	float mfOrigFOV;

	// Coop Options (story-scriptable; see the public accessors)
	bool mbCoopOptCrouchBoost;
	bool mbCoopOptPlayerCollision;
	bool mbCoopOptMonsterKilling;
	float mfCoopOptMonsterDamageMul;
	bool mbCoopOptGlobalLantern;

	/**
	 * Player-vs-player collision proxies ("blockers"): kinematic mass-0 boxes
	 * that only CHARACTER movement can see (SetCollide(false) hides them from
	 * props/ragdolls/Newton contacts; SetCollideCharacter(true) keeps them in
	 * the character movement solve). Each player's own char body masks out its
	 * own blocker's collide-flag bit, so nobody collides with themselves.
	 *
	 * Per player: a FULL body box (the Allow Player Collision option) and a
	 * thin HEAD platform (the crouch-boost option) that only engages when the
	 * other player comes down on it from above -- so walking into each other
	 * stays free while landing on a crouched partner gives real, standable,
	 * jump-off-able ground. Index = the player the blocker REPRESENTS.
	 */
	void UpdateCoopPlayerBlockers(float afTimeStep);
	void AbandonCoopPlayerBlockers();	//null the pointers; the physics world owns the bodies

	iPhysicsBody *mpCoopBlockerFull[2];
	iPhysicsBody *mpCoopBlockerHead[2];
	iPhysicsWorld *mpCoopBlockerWorld;
	bool mbCoopBoostRiding[2];			//[i] = player i is currently stood on the other's head

	// Background capture state (valid between Prepare/RestoreBackgroundCapture)
	eBackgroundCaptureMode mCaptureMode;
	cCamera *mpCaptureSavedCamera;		// main viewport's camera before a P1Full/P2Full capture

	// Dual-monitor menu mirror (GUI-only viewport over P2's monitor)
	cViewport *mpDualMenuMirrorViewport;
	cMapHandlerSoundCallback* mpSoundCallback;

	bool mbPausedSoundsAndMusic;

	bool mbUpdateActive;

	bool mbShowCommentary;

	iPostEffect *mpPostEffect_Bloom;
	iPostEffect *mpPostEffect_ImageTrail;
	iPostEffect *mpPostEffect_Sepia;
	iPostEffect *mpPostEffect_RadialBlur;

	// Co-op post effects.
	//
	// Enabled for left/right and top/bottom split, disabled for dual monitor --
	// see the comment in SetupCoopViewportPostEffects() for why the dual-monitor
	// window breaks the engine's UV math for P2 specifically. When they are off,
	// flashback sepia still reaches both players as a hud tint via
	// cLuxEffect_SepiaColor::DrawCoopOverlay().
	//
	// Built once on first use and never destroyed, so toggling coop or the split
	// layout does not leak a composite each time.
	void SetupCoopViewportPostEffects(cViewport *apViewport, bool abPlayer2);

	cPostEffectComposite *mpCoopPostEffectComp_P1;
	cPostEffectComposite *mpCoopPostEffectComp_P2;

	// Per-player insanity distortion on the split composites; built with them,
	// never destroyed (same lifetime policy as the composites).
	cLuxPostEffect_Insanity *mpCoopInsanity_P1;
	cLuxPostEffect_Insanity *mpCoopInsanity_P2;


	cLuxMapHandler_ChangeMap mMapChangeData;

	cLuxSavedGameMapCollection *mpSavedGame;
};

//----------------------------------------------


#endif // LUX_MAP_HANDLER_H