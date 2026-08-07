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

#ifndef LUX_INPUT_HANDLER_H
#define LUX_INPUT_HANDLER_H

//----------------------------------------------

#include "LuxBase.h"
#include "LuxTypes.h"

//----------------------------------------------

class cLuxAction
{	
public:
	cLuxAction() : msName(""){}
	cLuxAction(const tString& asName, 
			   int alId, 
			   bool abConfigurable, 
			   eLuxActionCategory aCat) : msName(asName), mlId(alId), mbConfigurable(abConfigurable), mCat(aCat){}

	tString msName;
	int mlId;
	bool mbConfigurable;
	eLuxActionCategory mCat;
};

typedef std::vector<cLuxAction*> tLuxActionVec;
typedef tLuxActionVec::iterator tLuxActionVecIt;

//----------------------------------------------

class cLuxInput
{
public:
	cLuxInput() : msInputType(""){}
	cLuxInput(const tString& asInputType, int alValue, int alActionId) : 
				msInputType(asInputType), mlValue(alValue), mlActionId(alActionId){}

	
    tString msInputType;
	int mlValue;
	int mlActionId;
};

typedef std::vector<cLuxInput*> tLuxInputVec;
typedef tLuxInputVec::iterator	tLuxInputVecIt;

//----------------------------------------------

class cLuxPlayer;

//----------------------------------------------


class cLuxInputHandler : public iLuxUpdateable
{
public:	
	cLuxInputHandler();
	~cLuxInputHandler();

	void LoadUserConfig();
	void SaveUserConfig();
	
	void OnStart();
	void Update(float afTimeStep);
	void Reset();
	void OnPostRender(float afFrameTime);

	tWString GetInputName(const tString& asActionName);

	void ChangeState(eLuxInputState aState);
	eLuxInputState GetState(){ return mState; }

	bool GetInvertMouse(){ return mbInvertMouse;}
	void SetInvertMouse(bool abX) { mbInvertMouse = abX; }

	bool GetSmoothMouse() { return mbSmoothMouse; }
	void SetSmoothMouse(bool abX) { mbSmoothMouse = abX; }

	float GetMouseSensitivity() { return mfMouseSensitivity; }
	void SetMouseSensitivity(float afX);

#ifdef USE_GAMEPAD
	bool GetInvertGamepadLook() { return mbGamepadLookInvert; }
	void SetInvertGamepadLook(bool abX) { mbGamepadLookInvert = abX; }

	float GetGamepadLookSensitivity() { return mfGamepadLookSensitivity; }
	void SetGamepadLookSensitivity(float afX);

	iGamepad* GetGamepad() { return mpPad; }
#endif

	cLuxAction*   GetActionByName(const tString& asName);
	cLuxAction*	  GetActionById(int alId);
	tLuxActionVec GetActionsByCategory(eLuxActionCategory aCat);

	tLuxInputVec GetDefaultInputsByActionId(int alId);

	void ResetSmoothMousePos();
	cVector2f GetSmoothMousePos(const cVector2f& avRelPosMouse);

#ifdef USE_GAMEPAD
	bool IsGamepadPresent();

	void AppDeviceWasPlugged();
	void AppDeviceWasRemoved();
#endif

	/**
	 * Is this action HELD, on the device that owns the menu that is open?
	 *
	 * The level-state twin of MenuActionTriggered, and public because the menus
	 * themselves need it: cLuxInventory decides whether a dragged item is still
	 * being held by reading eLuxAction_UIPrimary, which is gamepad A as well as
	 * Return -- so P2 resting a thumb on jump used to pin an item to P1's cursor.
	 *
	 * Reads nothing destructively: no edge is consumed, so the other player's
	 * press still reaches their own gameplay input this frame.
	 */
	bool MenuActionHeld(int alAction);

private:
	void UpdateGlobalInput();
	bool UpdateGamepadUIInput();
	
	void UpdateGameInput();
	void UpdateGamePlayerInput();
	void UpdatePlayer2Input();
	/**
	 * Coop: while one player is in a menu, run the OTHER player's gameplay
	 * input. Without this the world ticking underneath is useless -- the other
	 * player would stand there unable to move.
	 */
	void UpdateCoopFreeRoamInput();
	// The player whose menu is currently open, or NULL if none is.
	cLuxPlayer* GetCoopMenuOwner();
	/**
	 * Coop: which device may touch the menu that is currently open.
	 *
	 * P1 plays on keyboard and mouse, P2 on the pad. The world no longer stops
	 * for a menu, so the free player is holding a live controller -- and without
	 * this their stick and buttons drive their partner's menu. Both return true
	 * when nothing is open, in single player, and for a world note that is on
	 * both halves (either player may dismiss that one).
	 */
	bool CoopMenuAcceptsKeyboard();
	bool CoopMenuAcceptsGamepad();
	/**
	 * BecameTriggerd() for a menu, honoured only if it came from the device that
	 * OWNS that menu.
	 *
	 * Actions merge their inputs -- Tab and gamepad Back are the same
	 * eLuxAction_Inventory -- so a pad press out in the world is indistinguishable
	 * from the owner's key at the action level. This walks the action's sub-inputs
	 * and asks which of them is actually firing.
	 */
	bool MenuActionTriggered(int alAction);
	/**
	 * Is a sub-input of this action, of the given device family, triggered RIGHT
	 * NOW? Level state, not an edge -- reading it consumes nothing.
	 *
	 * This is the load-bearing part: cAction::BecameTriggerd() CLEARS the edge as
	 * it reports it (the codebase already relies on that to swallow clicks). So a
	 * menu handler that reads an action to decide it does not want it has already
	 * eaten it, and the other player's press never reaches their gameplay input.
	 */
	bool SubActionTriggeredForDevice(int alAction, bool abGamepad);
	void UpdateGameMessageInput();
	void UpdateGameEffectInput();

	// Mouse position/motion for GUI input. In coop the inventory GUI renders
	// inside the opening player's viewport rect, but cGui::SendMousePos maps
	// the absolute position over the FULL screen — these remap the mouse into
	// the inventory viewport so clicks land where the cursor is drawn.
	cVector2l GetGuiMouseAbsPos();
	cVector2l GetGuiMouseRelPos();
	
	void UpdatePreMenuInput();
	void UpdateMainMenuInput();
	void UpdateInventoryInput();
	void UpdateJournalInput();
	void UpdateDebugInput();
	void UpdateCreditsInput();
	void UpdateDemoEndInput();
	void UpdateLoadScreenInput();

	bool CurrentStateSendsInputToGui();

	void CreateActions();

	void CreateSubAction(cAction *apAction,const tStringVec& avType, int alValue);

	tStringVec GetInputValueStrings(const tString& asX);

	bool CreateSubActionFromInputString(cAction* apAction, const tString& asInputString);

	bool ShowMouseOnMouseInput();

#ifdef USE_GAMEPAD
	void SetUpGamepad();

	// Reset Player 2's gamepad edge-detection state to the current physical
	// button state. Called on every input-state change so that buttons held
	// across a (paused) menu transition don't register spurious presses or
	// releases on the first frame back in gameplay.
	void ResetPlayer2InputState();
#endif

	cGraphics *mpGraphics;
	cInput *mpInput;

	cLuxPlayer *mpPlayer;

	eLuxInputState mState;

	bool mbSmoothMouse;
	bool mbInvertMouse;

	double mfMouseActiveAt;

	float mfMouseSensitivity;

	int mlMaxSmoothMousePos;
	float mfPrevSmoothMousePosMul;
	tVector2fList mlstSmoothMousePos;

	cVector2l mvLastAbsMousePos;

	// Player 1 keyboard edge-detection state, used ONLY while coop is active with a
	// gamepad attached. See the comment in UpdateGamePlayerInput(): the shared
	// eLuxAction_Jump/Crouch/Run objects are bound to both the keyboard and the pad,
	// so P2's pad starves P1's action edges. Declared outside USE_GAMEPAD because the
	// non-gamepad build still compiles the branch that clears them.
	bool mbP1_Jump_WasDown;
	bool mbP1_Crouch_WasDown;
	bool mbP1_Run_WasDown;

	// Possession toggle. eKey_H is read raw, so it needs its own edge state --
	// the action bound to that key (eLuxAction_Holster) is a debug self-damage
	// cheat and its edge gets drained rather than used.
	bool mbPossessKeyWasDown;

	// Enemy morph, keys 1-4, likewise read raw and likewise needing their own
	// edges. Unlike H these keys are bound to no eLuxAction_ at all, so there is
	// nothing to drain. Sized to eLuxMorphType_LastEnum; the static assert that
	// would say so lives at the loop in UpdateGamePlayerInput, where the enum is
	// in scope -- this header does not include LuxPlayerHelpers.h.
	bool mvMorphKeyWasDown[4];

#ifdef USE_GAMEPAD
	float mfGamepadWalkSensitivity;
	float mfGamepadLookSensitivity;
	bool mbGamepadLookInvert;
	iGamepad* mpPad;
	bool mbGamepadUIInput;

	// Player 2 gamepad edge-detection state. Kept as members (instead of
	// function-local statics) so ChangeState can reset them — see
	// ResetPlayer2InputState().
	bool mbP2_A_WasDown;
	bool mbP2_B_WasDown;
	bool mbP2_X_WasDown;
	bool mbP2_LS_WasDown;
	bool mbP2_GunTrigger_WasDown;
	bool mbP2_Y_WasDown;
	bool mbP2_RS_WasDown;
	bool mbP2_Run_WasDown;
	bool mbP2_Back_WasDown;		// opens P2 inventory (gameplay state)
	bool mbP2_InvBack_WasDown;	// closes P2 inventory (inventory state)
	bool mbP2_JournalBack_WasDown;	// closes a P2-owned journal (journal state)
	bool mbPadUIHeld[16];			// UI buttons the PAD pressed, so only those are released
	bool mbP1GameInputRan;			// this frame -- so free roam never doubles or skips
	bool mbP2GameInputRan;
	bool mbP2_BothSticks_WasDown;	// both stick buttons = debug teleport to P1
#endif
};

//----------------------------------------------


#endif // LUX_INPUT_HANDLER_H