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

#include <vector>
#include <map>

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

	/**
	 * Coop: keep the player who is NOT in a menu alive.
	 *
	 * cLuxPlayer lives in the "Default" update container. Open a menu and the
	 * container becomes "Journal" or "Inventory", so cLuxPlayer::Update and
	 * ::OnDraw stop being called AT ALL -- for both players, because Player 2 is
	 * drawn from inside Player 1's pass.
	 *
	 * That is correct in single player, where a menu is a pause. It is wrong here:
	 * the other player is still standing in a corridor. Their move state never
	 * runs, so they cannot walk or look however much input reaches them; their HUD
	 * never draws, so the grab and lever icons vanish off doors they are staring
	 * at; and none of it comes back until the menu closes.
	 *
	 * This handler is a GLOBAL module -- it runs in every container -- so it is
	 * the one place that can still tick them. UpdateCoopFreeRoamInput already does
	 * exactly this for their input; these do it for the rest of them.
	 */
	void OnDraw(float afFrameTime);
	void PostUpdate(float afTimeStep);

	// The player whose menu is currently open, or NULL if none is. Public because
	// cLuxPlayer::OnDraw has to know whether its partner is in one.
	cLuxPlayer* GetCoopMenuOwner();
	bool CoopMenuIsShared();
	/** The player who is NOT in a menu, or NULL when that question has no answer. */
	cLuxPlayer* GetCoopFreeRoamPlayer();

	/**
	 * SDL's mouse motion for this tick, read ONCE and handed out to everybody.
	 *
	 * cMouseSDL::GetRelPosition() is DESTRUCTIVE -- it returns the accumulated
	 * delta and zeroes it -- so it is not a getter, it is a take. Whoever calls it
	 * first this frame gets the motion and everyone after them gets nothing.
	 *
	 * That is survivable in single player, where the menu path and the gameplay
	 * path never run in the same frame. In co-op they do: one player is in a menu
	 * while the other is still walking around, so the menu cursor took the delta
	 * and the free player's mouselook was handed a zero -- and then SUBTRACTED
	 * Player 2's raw motion from it, which drove Player 1's view backwards
	 * whenever Player 2 moved their mouse. "He is moving my look around."
	 */
	const cVector2l& GetFrameMouseRel(){ return mvFrameMouseRel; }

	/**
	 * Player 2's own pointer while a shared note is up, in apSet's virtual
	 * coordinates, ready to draw. False when there is no second pointer --
	 * single player, pad co-op, or any menu that belongs to one player and
	 * therefore already has its cursor.
	 */
	bool GetCoopSharedCursorP2Virtual(cGuiSet *apSet, cVector2f &avOut);
	cVector2l mvFrameMouseRel;

	/**
	 * Is the handle we think is Player 2's mouse actually a mouse that has sent
	 * something?
	 *
	 * Until it is, every mouse question asked of it answers "no motion, no
	 * buttons" -- and the dangerous half of that is the EXCLUDING form, which then
	 * excludes nothing: Player 2 holding a button reads as Player 1 holding it, so
	 * Player 1 never gets a fresh press and cannot click anything. Better to share
	 * the mouse for a few seconds than to take Player 1's away.
	 */
	bool P2RawMouseKnown();
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
	 * Coop: is the open menu one that BOTH players are being shown?
	 *
	 * A note found in the world under forced co-op, or one marked ReadByBoth by
	 * the author. The world is stopped for it, neither player is out there
	 * playing, and either of them may turn the page or put it down -- so every
	 * device is allowed in, and nobody free-roams behind it.
	 */
	/** Coop: may Player 2's own keyboard/mouse drive the open menu? */
	bool CoopMenuAcceptsP2Raw();
	/** The viewport the open menu is drawn in, or NULL if no menu is open. */
	cViewport* GetCoopMenuViewport();
	/**
	 * Coop: advance the menu cursor from the OWNER's device alone.
	 *
	 * There is one GUI cursor and, in this mode, two mice. SDL adds them
	 * together, so without this the free player's mouse drags their partner's
	 * pointer off whatever they were about to click -- which is why nobody could
	 * hit the arrow on a note. Kept in screen pixels, the same space
	 * GetGuiMouseAbsPos works in, and confined to the owner's own viewport.
	 */
	void UpdateCoopMenuCursor();
	/** Player 2's own pointer for a shared note. Called by UpdateCoopMenuCursor. */
	void UpdateCoopSharedCursorP2(bool abShared, int alP2X, int alP2Y,
								  const cVector2l &avMin, const cVector2l &avMax);
	/** Feed Player 2's raw device into the GUI while they own the open menu. */
	void SendP2RawInputToGui();
	/** True when one of Player 2's GUI mouse buttons changed state this frame. */
	bool P2RawGuiHasButtonEdge();
	/** Seed P2's menu-side edge trackers from the live device on a state change. */
	void ResetPlayer2RawGuiState();
	/**
	 * A GUI click, from whoever is entitled to make it.
	 *
	 * Filtered to Player 1's devices normally, because Player 2 is out in the
	 * world with a live mouse and SDL cannot tell the two apart. Unfiltered for a
	 * note both of them are reading, where either may turn the page.
	 */
	bool GuiActionBecame(int alAction);
	bool GuiActionWas(int alAction);
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
	/** The same, for Player 2's second pointer during a shared note. */
	cVector2l GetGuiMouseAbsPosP2();

	//The owner-driven cursor UpdateCoopMenuCursor maintains. Screen pixels.
	//mbCoopCursorValid is false whenever there is nothing to separate, and the
	//two functions above then behave exactly as they always did.
	cVector2l mvCoopCursorPos;
	cVector2l mvCoopCursorRel;
	bool mbCoopCursorValid;

	//////////////////////////////////////////////////////////////////////////
	// PLAYER 2'S OWN POINTER, for a note they are both reading.
	//
	// The one above is Player 1's. A shared note is one cGuiSet with one
	// cursor in it, so this second position is integrated here from Player 2's
	// device alone, drawn by cLuxJournal, and handed to the GUI only for the
	// instant one of Player 2's clicks is delivered. Neither mouse can reach
	// the other's pointer at any point.
	cVector2l mvCoopCursorPosP2;
	bool mbCoopCursorP2Valid;
	
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

	// Player 2 on a second keyboard and mouse. Edge state for every action that is
	// a press rather than a hold, mirroring the gamepad members above -- the raw
	// layer reports level state only, so the edges are ours to keep.
	bool mbP2Raw_Jump_WasDown;
	bool mbP2Raw_Run_WasDown;
	bool mbP2Raw_Crouch_WasDown;
	bool mbP2Raw_Lantern_WasDown;
	bool mbP2Raw_Interact_WasDown;
	bool mbP2Raw_Attack_WasDown;
	bool mbP2Raw_Inventory_WasDown;
	bool mbP2Raw_Journal_WasDown;
	bool mbP2Raw_QuestLog_WasDown;
	bool mbP2Raw_RecentText_WasDown;
	bool mbP2Raw_NoteAdvance_WasDown;

	//Effects that PAUSE the player they belong to -- the emotion-stone vision.
	//Kept apart from the two members above and tracked on every frame, active or
	//not: the click that starts a vision is still held when the vision begins, so
	//a tracker that only ran during the pause would read that same held button as
	//the press that skips it, and the text would be gone before it was drawn.
	//Shared by the raw and gamepad paths, which never run in the same frame.
	bool mbP2Fx_Primary_WasDown;
	bool mbP2Fx_Secondary_WasDown;

	//Player 2's double click, which the inventory needs and their raw stream does
	//not have -- see SendP2RawInputToGui.
	double mfP2LastGuiClickTime;

	//Player 2's device while it is driving a MENU rather than their character.
	bool mbP2Raw_GuiLeft_WasDown;
	bool mbP2Raw_GuiRight_WasDown;
	bool mbP2Raw_GuiExit_WasDown;
	bool mbP2Raw_GuiInventory_WasDown;
	bool mbP2Raw_GuiJournal_WasDown;

	/** Player 2's device, or NULL when P2 is not on raw input at all. */
	void* GetP2RawDevice();

public:
	/**
	 * P1's view of a key: excludes P2's device once P2 owns one.
	 *
	 * Public because it is not only this class's business. Anything outside
	 * that reads the keyboard directly is asking SDL, and SDL keeps one logical
	 * key state for the whole machine -- so it is answering for both players at
	 * once. cLuxPlayer::UpdateNoclip was the one that showed it: Player 2
	 * walking around flew Player 1 across the level.
	 */
	bool P1KeyIsDown(eKey aKey);

private:

	//////////////////////////////////////////////////////////////////////////
	// PLAYER 1'S ACTIONS, WITH PLAYER 2'S DEVICE TAKEN OUT
	//
	// cInput's actions are fed by SDL, which has one keyboard and one mouse for
	// the whole machine -- so every action Player 2 triggers fires for Player 1
	// too. Filtering only the movement keys was not enough: interact, attack,
	// jump, crouch, the lantern, even the inventory all come through here.
	//
	// These are drop-in replacements for mpInput->IsTriggerd / BecameTriggerd /
	// WasTriggerd, and pass straight through whenever Player 2 is not on a
	// keyboard of their own.
	//
	// The edges are recomputed rather than borrowed from cAction. cAction's are
	// derived from the aggregate, so Player 2 holding a key would swallow Player
	// 1's press of the same one -- no new edge, because the action never went up.
	//////////////////////////////////////////////////////////////////////////
	void UpdateP1ActionFilter();
	bool P1ActionLevel(int alAction);

	bool P1IsTriggerd(int alAction);
	bool P1BecameTriggerd(int alAction);
	bool P1WasTriggerd(int alAction);

	bool mbP1ActionFilterActive;
	std::vector<bool> mvP1ActionDown;
	std::vector<bool> mvP1ActionBecame;
	std::vector<bool> mvP1ActionReleased;

	/**
	 * Work out WHICH MOUSE is Player 2's, without anybody having to say.
	 *
	 * Picking Player 2's keyboard says nothing about their mouse -- they are two
	 * devices with two handles -- and getting it wrong is completely silent:
	 * every mouse question asked of a keyboard handle answers "no motion, no
	 * buttons", which looks exactly like the whole feature being broken.
	 *
	 * The signal is co-activity. A mouse that keeps moving while Player 2's
	 * OWN keyboard has a movement key held is Player 2's mouse; Player 1 is not
	 * pressing keys on Player 2's keyboard. Scored over many frames and only
	 * accepted when one device is clearly ahead, so a single frame where the
	 * other player happened to twitch cannot decide it.
	 *
	 * Never overrides a device picked by hand in the debug menu.
	 */
	void UpdateP2MousePairing();
	std::map<void*, int> mmapP2MouseScore;



	/** Drives Player 2 from their own keyboard and mouse. True if it ran. */
	bool UpdatePlayer2RawInput();
	void ResetPlayer2RawInputState();

	/**
	 * Player 2's two buttons while an effect has them frozen. Called from both
	 * P2 paths BEFORE their IsActive() early-out, which is the whole point: a
	 * vision sets P2 inactive, so the ordinary button pass never runs and P2 had
	 * no way at all to press through their own text.
	 */
	void UpdatePlayer2EffectInput(bool abPrimaryDown, bool abSecondaryDown);

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