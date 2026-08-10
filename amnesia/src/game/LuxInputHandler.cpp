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

#include "LuxInputHandler.h"

#include "impl/ImGuiConsole.h"

#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxPlayerHelpers.h"

#include "LuxPreMenu.h"
#include "LuxMainMenu.h"
#include "LuxCredits.h"
#include "LuxDemoEnd.h"

#include "LuxInventory.h"
#include "LuxJournal.h"
#include "LuxMessageHandler.h"
#include "LuxMapHandler.h"
#include "LuxSaveHandler.h"
#include "LuxEffectHandler.h"
#include "LuxConfigHandler.h"
#include "LuxLoadScreenHandler.h"

#include "LuxDebugHandler.h"

#include "impl/ImGuiDebugMenu.h"
#include "impl/LowLevelInputSDL.h"
#include "input/ActionKeyboard.h"
#include "input/ActionMouseButton.h"

//////////////////////////////////////////////////////////////////////////
// ACTION LISTS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

static cLuxAction gvLuxActions[] =
{
	cLuxAction("Exit",eLuxAction_Exit,				false, eLuxActionCategory_System),
	cLuxAction("ExitDirect",eLuxAction_ExitDirect,	false, eLuxActionCategory_System),
	cLuxAction("ScreenShot",eLuxAction_ScreenShot,	false, eLuxActionCategory_System),
	cLuxAction("PrintInfo",eLuxAction_PrintInfo,	false, eLuxActionCategory_System),

	cLuxAction("LeftClick",eLuxAction_LeftClick,	false, eLuxActionCategory_System),
	cLuxAction("MiddleClick",eLuxAction_MiddleClick,false, eLuxActionCategory_System),
	cLuxAction("RightClick",eLuxAction_RightClick,	false, eLuxActionCategory_System),
	cLuxAction("ScrollUp",eLuxAction_ScrollUp,	false, eLuxActionCategory_System),
	cLuxAction("ScrollDown",eLuxAction_ScrollDown,	false, eLuxActionCategory_System),
	cLuxAction("MouseButton6",eLuxAction_MouseButton6Click,	false, eLuxActionCategory_System),
	cLuxAction("MouseButton7",eLuxAction_MouseButton7Click,	false, eLuxActionCategory_System),
	cLuxAction("MouseButton8",eLuxAction_MouseButton8Click,	false, eLuxActionCategory_System),
	cLuxAction("MouseButton9",eLuxAction_MouseButton9Click,	false, eLuxActionCategory_System),

	cLuxAction("UIArrowUp", eLuxAction_UIArrowUp,		false, eLuxActionCategory_System),
	cLuxAction("UIArrowLeft", eLuxAction_UIArrowLeft,		false, eLuxActionCategory_System),
	cLuxAction("UIArrowDown", eLuxAction_UIArrowDown,	false, eLuxActionCategory_System),
	cLuxAction("UIArrowRight", eLuxAction_UIArrowRight,		false, eLuxActionCategory_System),

	cLuxAction("UIPrimary", eLuxAction_UIPrimary,		false, eLuxActionCategory_System),
	cLuxAction("UISecondary", eLuxAction_UISecondary,		false, eLuxActionCategory_System),
	cLuxAction("UIPrevPage", eLuxAction_UIPrevPage,		false, eLuxActionCategory_System),
	cLuxAction("UINextPage", eLuxAction_UINextPage,		false, eLuxActionCategory_System),
	cLuxAction("UIDelete", eLuxAction_UIDelete,		false, eLuxActionCategory_System),
	cLuxAction("UIClear", eLuxAction_UIClear,		false, eLuxActionCategory_System),

	cLuxAction("OpenDebug",eLuxAction_OpenDebug,	false, eLuxActionCategory_System),
	cLuxAction("ReloadMap",eLuxAction_ReloadMap,	false, eLuxActionCategory_System),
	cLuxAction("QuickSave",eLuxAction_QuickSave,	false, eLuxActionCategory_System),
	cLuxAction("QuickLoad",eLuxAction_QuickLoad,	false, eLuxActionCategory_System),
	cLuxAction("FastForward",eLuxAction_FastForward,	false, eLuxActionCategory_System),

	cLuxAction("Inventory",eLuxAction_Inventory,	true, eLuxActionCategory_Misc),
	cLuxAction("Journal",eLuxAction_Journal,		true, eLuxActionCategory_Misc),
	cLuxAction("QuestLog",eLuxAction_QuestLog,		true, eLuxActionCategory_Misc),
	cLuxAction("RecentText", eLuxAction_RecentText, true, eLuxActionCategory_Misc),
	cLuxAction("CrosshairToggle", eLuxAction_CrosshairToggle, true, eLuxActionCategory_Misc),
	
	cLuxAction("Forward",eLuxAction_Forward,	true, eLuxActionCategory_Movement),
	cLuxAction("Backward",eLuxAction_Backward,	true, eLuxActionCategory_Movement),
	cLuxAction("Right",eLuxAction_Right,		true, eLuxActionCategory_Movement),
	cLuxAction("Left",eLuxAction_Left,			true, eLuxActionCategory_Movement),

	cLuxAction("LeanRight",eLuxAction_LeanRight,true, eLuxActionCategory_Movement),
	cLuxAction("LeanLeft",eLuxAction_LeanLeft,	true, eLuxActionCategory_Movement),

#ifdef USE_GAMEPAD
	cLuxAction("Lean",eLuxAction_Lean,	true, eLuxActionCategory_Movement),
	cLuxAction("ZoomOut", eLuxAction_ZoomOut, false, eLuxActionCategory_Action),
	cLuxAction("ZoomIn", eLuxAction_ZoomIn, false, eLuxActionCategory_Action),
#endif

	cLuxAction("Attack",eLuxAction_Attack,		true, eLuxActionCategory_Action),
	cLuxAction("Interact",eLuxAction_Interact,	true, eLuxActionCategory_Action),
	cLuxAction("Ignite",eLuxAction_Ignite,		false, eLuxActionCategory_Action),
	cLuxAction("Rotate",eLuxAction_Rotate,		true, eLuxActionCategory_Action),
	cLuxAction("Holster",eLuxAction_Holster,	false, eLuxActionCategory_Action),
	cLuxAction("Lantern",eLuxAction_Lantern,	true, eLuxActionCategory_Action),
	cLuxAction("DebugGun",eLuxAction_DebugGun,	true, eLuxActionCategory_Action),

	cLuxAction("Run",eLuxAction_Run,			true, eLuxActionCategory_Movement),
	cLuxAction("Crouch",eLuxAction_Crouch,		true, eLuxActionCategory_Movement),
	cLuxAction("Jump",eLuxAction_Jump,			true, eLuxActionCategory_Movement),


	cLuxAction()
};

//-----------------------------------------------------------------------

// This is a list of all inputs, note that there can be several inputs per action!! (and this is the reason for having two arrays!!)
// Also note though that the current save/load of actions assume that there are no duplicates, and hence DO NOT add duplicates here
//until the key config load is updated!! (Remove this message then).
static cLuxInput gvLuxInputs[] =
{
	cLuxInput("Keyboard", eKey_Escape, eLuxAction_Exit),
	cLuxInput("Keyboard", eKey_F12, eLuxAction_ExitDirect),
	cLuxInput("Keyboard", eKey_F8, eLuxAction_ScreenShot),
	cLuxInput("Keyboard", eKey_P, eLuxAction_PrintInfo),

	cLuxInput("MouseButton", eMouseButton_Left, eLuxAction_LeftClick),
	cLuxInput("MouseButton", eMouseButton_Middle, eLuxAction_MiddleClick),
	cLuxInput("MouseButton", eMouseButton_Right, eLuxAction_RightClick),
	cLuxInput("MouseButton", eMouseButton_WheelUp, eLuxAction_ScrollUp),
	cLuxInput("MouseButton", eMouseButton_WheelDown, eLuxAction_ScrollDown),
	cLuxInput("MouseButton", eMouseButton_Button6, eLuxAction_MouseButton6Click),
	cLuxInput("MouseButton", eMouseButton_Button7, eLuxAction_MouseButton7Click),
	cLuxInput("MouseButton", eMouseButton_Button8, eLuxAction_MouseButton8Click),
	cLuxInput("MouseButton", eMouseButton_Button9, eLuxAction_MouseButton9Click),

#ifdef USE_GAMEPAD
#if USE_SDL2
	cLuxInput("GamepadButton", eGamepadButton_DpadUp, eLuxAction_UIArrowUp),
	cLuxInput("GamepadButton", eGamepadButton_DpadDown, eLuxAction_UIArrowDown),
	cLuxInput("GamepadButton", eGamepadButton_DpadLeft, eLuxAction_UIArrowLeft),
	cLuxInput("GamepadButton", eGamepadButton_DpadRight, eLuxAction_UIArrowRight),

	cLuxInput("GamepadAxis.Axis LeftY", eGamepadAxisRange_Negative, eLuxAction_UIArrowUp),
	cLuxInput("GamepadAxis.Axis LeftX", eGamepadAxisRange_Positive, eLuxAction_UIArrowRight),
	cLuxInput("GamepadAxis.Axis LeftY", eGamepadAxisRange_Positive, eLuxAction_UIArrowDown),
	cLuxInput("GamepadAxis.Axis LeftX", eGamepadAxisRange_Negative, eLuxAction_UIArrowLeft),

	cLuxInput("GamepadButton", eGamepadButton_A, eLuxAction_UIPrimary),
	cLuxInput("GamepadButton", eGamepadButton_B, eLuxAction_UISecondary),
	cLuxInput("GamepadButton", eGamepadButton_LeftShoulder, eLuxAction_UIPrevPage),
	cLuxInput("GamepadButton", eGamepadButton_RightShoulder, eLuxAction_UINextPage),
	cLuxInput("GamepadAxis.Axis LeftTrigger", eGamepadAxisRange_Positive, eLuxAction_UIPrevPage),
	cLuxInput("GamepadAxis.Axis RightTrigger", eGamepadAxisRange_Positive, eLuxAction_UINextPage),

	cLuxInput("GamepadButton", eGamepadButton_X, eLuxAction_UIDelete),
	cLuxInput("GamepadButton", eGamepadButton_Y, eLuxAction_UIClear),
#else
	cLuxInput("GamepadHat.Hat 0", eGamepadHatState_Up, eLuxAction_UIArrowUp),
	cLuxInput("GamepadHat.Hat 0", eGamepadHatState_Down, eLuxAction_UIArrowDown),
	cLuxInput("GamepadHat.Hat 0", eGamepadHatState_Left, eLuxAction_UIArrowLeft),
	cLuxInput("GamepadHat.Hat 0", eGamepadHatState_Right, eLuxAction_UIArrowRight),

	cLuxInput("GamepadAxis.Axis 1", eGamepadAxisRange_Negative, eLuxAction_UIArrowUp),
	cLuxInput("GamepadAxis.Axis 0", eGamepadAxisRange_Positive, eLuxAction_UIArrowRight),
	cLuxInput("GamepadAxis.Axis 1", eGamepadAxisRange_Positive, eLuxAction_UIArrowDown),
	cLuxInput("GamepadAxis.Axis 0", eGamepadAxisRange_Negative, eLuxAction_UIArrowLeft),

	cLuxInput("GamepadButton", eGamepadButton_0, eLuxAction_UIPrimary),
	cLuxInput("GamepadButton", eGamepadButton_1, eLuxAction_UISecondary),
	cLuxInput("GamepadButton", eGamepadButton_4, eLuxAction_UIPrevPage),
	cLuxInput("GamepadButton", eGamepadButton_5, eLuxAction_UINextPage),
	cLuxInput("GamepadAxis.Axis 2", eGamepadAxisRange_Positive, eLuxAction_UIPrevPage),
	cLuxInput("GamepadAxis.Axis 2", eGamepadAxisRange_Negative, eLuxAction_UINextPage),

	cLuxInput("GamepadButton", eGamepadButton_2, eLuxAction_UIDelete),
	cLuxInput("GamepadButton", eGamepadButton_3, eLuxAction_UIClear),
#endif
#endif
	
	cLuxInput("Keyboard", eKey_Return, eLuxAction_UIPrimary),

	cLuxInput("Keyboard", eKey_F1, eLuxAction_OpenDebug),
	cLuxInput("Keyboard", eKey_F2, eLuxAction_ReloadMap),
	cLuxInput("Keyboard", eKey_F4, eLuxAction_QuickSave),
	cLuxInput("Keyboard", eKey_F5, eLuxAction_QuickLoad),
	cLuxInput("Keyboard", eKey_F3, eLuxAction_FastForward),
	
	cLuxInput("Keyboard", eKey_Tab, eLuxAction_Inventory),
	cLuxInput("Keyboard", eKey_J, eLuxAction_Journal),
	cLuxInput("Keyboard", eKey_M, eLuxAction_QuestLog),
	cLuxInput("Keyboard", eKey_N, eLuxAction_RecentText),
	cLuxInput("Keyboard", eKey_X, eLuxAction_CrosshairToggle),

	cLuxInput("Keyboard", eKey_W, eLuxAction_Forward),
	cLuxInput("Keyboard", eKey_S, eLuxAction_Backward),
	cLuxInput("Keyboard", eKey_D, eLuxAction_Right),
	cLuxInput("Keyboard", eKey_A, eLuxAction_Left),

	cLuxInput("Keyboard", eKey_E, eLuxAction_LeanRight),
	cLuxInput("Keyboard", eKey_Q, eLuxAction_LeanLeft),

	cLuxInput("Keyboard", eKey_LeftAlt, eLuxAction_Lean),

	cLuxInput("MouseButton", eMouseButton_Right, eLuxAction_Attack),
	cLuxInput("MouseButton", eMouseButton_Left, eLuxAction_Interact),
	cLuxInput("Keyboard", eKey_C, eLuxAction_Ignite),
	cLuxInput("Keyboard", eKey_R, eLuxAction_Rotate),
	cLuxInput("Keyboard", eKey_H, eLuxAction_Holster),
	cLuxInput("Keyboard", eKey_F, eLuxAction_Lantern),
	cLuxInput("Keyboard", eKey_G, eLuxAction_DebugGun),
	
	cLuxInput("Keyboard", eKey_LeftShift, eLuxAction_Run),
	cLuxInput("Keyboard", eKey_LeftCtrl, eLuxAction_Crouch),
	cLuxInput("Keyboard", eKey_Space, eLuxAction_Jump),

	///////////////////////////////////////////////////////////////////////
	// Gamepad layout
	// ----------Buttons---------
	// 0 --> A
	// 1 --> B
	// 2 --> X
	// 3 --> Y
	// 4 --> LB
	// 5 --> RB
	// 6 --> Back
	// 7 --> Start
	// 8 --> LA
	// 9 --> RA
	// ------------Hat-------------
	// Up --> DPAD-Up
	// Down --> DPAD-Down
	// Left --> DPAD-Left
	// Right --> DPAD-Right
	// ------------Axis-------------
	// 0 --> (-) Left, (+) Right
	// 1 --> (-) Forward, (+) Back
	// 2 --> (-) Look-Down, (+) Look-Up
	// 3 --> (-) Look-Left, (+) Look-Right
	// 4 --> (-) LTrigger, (+) RTrigger

#ifdef USE_GAMEPAD
#if USE_SDL2
	cLuxInput("GamepadAxis.Axis LeftY", eGamepadAxisRange_Negative, eLuxAction_Forward),
	cLuxInput("GamepadAxis.Axis LeftX", eGamepadAxisRange_Positive, eLuxAction_Right),
	cLuxInput("GamepadAxis.Axis LeftY", eGamepadAxisRange_Positive, eLuxAction_Backward),
	cLuxInput("GamepadAxis.Axis LeftX", eGamepadAxisRange_Negative, eLuxAction_Left),

	cLuxInput("GamepadButton", eGamepadButton_A, eLuxAction_Jump),
	cLuxInput("GamepadButton", eGamepadButton_B, eLuxAction_Crouch),
	cLuxInput("GamepadButton", eGamepadButton_X, eLuxAction_Lantern),
	cLuxInput("GamepadButton", eGamepadButton_Y, eLuxAction_Journal),
	cLuxInput("GamepadButton", eGamepadButton_DpadRight, eLuxAction_QuestLog),
	cLuxInput("GamepadButton", eGamepadButton_DpadLeft, eLuxAction_RecentText),
	cLuxInput("GamepadButton", eGamepadButton_Back, eLuxAction_Inventory),
	cLuxInput("GamepadButton", eGamepadButton_LeftShoulder, eLuxAction_Attack),
	cLuxInput("GamepadButton", eGamepadButton_RightShoulder, eLuxAction_Interact),
	cLuxInput("GamepadButton", eGamepadButton_Start, eLuxAction_Exit),
	cLuxInput("GamepadButton", eGamepadButton_LeftStick, eLuxAction_CrosshairToggle),
	cLuxInput("GamepadButton", eGamepadButton_RightStick, eLuxAction_Rotate),
	cLuxInput("GamepadAxis.Axis LeftTrigger", eGamepadAxisRange_Positive, eLuxAction_Run),
	cLuxInput("GamepadAxis.Axis RightTrigger", eGamepadAxisRange_Positive, eLuxAction_Lean),

	cLuxInput("GamepadButton", eGamepadButton_DpadUp, eLuxAction_ZoomOut),
	cLuxInput("GamepadButton", eGamepadButton_DpadDown, eLuxAction_ZoomIn),
#else
	cLuxInput("GamepadAxis.Axis 1", eGamepadAxisRange_Negative, eLuxAction_Forward),
	cLuxInput("GamepadAxis.Axis 0", eGamepadAxisRange_Positive, eLuxAction_Right),
	cLuxInput("GamepadAxis.Axis 1", eGamepadAxisRange_Positive, eLuxAction_Backward),
	cLuxInput("GamepadAxis.Axis 0", eGamepadAxisRange_Negative, eLuxAction_Left),

	cLuxInput("GamepadButton", eGamepadButton_0, eLuxAction_Jump),
	cLuxInput("GamepadButton", eGamepadButton_1, eLuxAction_Crouch),
	cLuxInput("GamepadButton", eGamepadButton_2, eLuxAction_Lantern),
	cLuxInput("GamepadButton", eGamepadButton_3, eLuxAction_Journal),
	cLuxInput("GamepadHat.Hat 0", eGamepadHatState_Right, eLuxAction_QuestLog),
	cLuxInput("GamepadHat.Hat 0", eGamepadHatState_Left, eLuxAction_RecentText),
	cLuxInput("GamepadButton", eGamepadButton_6, eLuxAction_Inventory),
	cLuxInput("GamepadButton", eGamepadButton_4, eLuxAction_Attack),
	cLuxInput("GamepadButton", eGamepadButton_5, eLuxAction_Interact),
	cLuxInput("GamepadButton", eGamepadButton_7, eLuxAction_Exit),
	cLuxInput("GamepadButton", eGamepadButton_8, eLuxAction_CrosshairToggle),
	cLuxInput("GamepadButton", eGamepadButton_9, eLuxAction_Rotate),
	cLuxInput("GamepadAxis.Axis 2", eGamepadAxisRange_Positive, eLuxAction_Run),
	cLuxInput("GamepadAxis.Axis 2", eGamepadAxisRange_Negative, eLuxAction_Lean),
	
	cLuxInput("GamepadHat.Hat 0", eGamepadHatState_Up, eLuxAction_ZoomOut),
	cLuxInput("GamepadHat.Hat 0", eGamepadHatState_Down, eLuxAction_ZoomIn),
#endif
#endif

	cLuxInput()
};

//-----------------------------------------------------------------------

// These are used for the Load/SaveUserConfig, to keep them in one place

static tString gvLuxInputPos[] =
{
	"Primary",
	"Secondary",

	""
};

static tString gsLuxInputStringSeparator = ".";

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxInputHandler::cLuxInputHandler() : iLuxUpdateable("LuxInputHandler")
{
	////////////////////////////////////
	// Get needed engine modules
	mpInput = gpBase->mpEngine->GetInput();
	mpGraphics = gpBase->mpEngine->GetGraphics();

	////////////////////////////////////
	// Game settings init
	mlMaxSmoothMousePos = gpBase->mpGameCfg->GetInt("Input","MaxSmoothMousePos",0);
	mfPrevSmoothMousePosMul = gpBase->mpGameCfg->GetFloat("Input","PrevSmoothMousePosMul",0);

#ifdef USE_GAMEPAD
	////////////////////////////////////
	// Set up gamepad
	SetUpGamepad();
#endif

	////////////////////////////////////
	// Create the actions
	CreateActions();

	////////////////////////////////////
	// Variable init
	mState = eLuxInputState_Game;
	mfMouseActiveAt = -1;

	mbP1_Jump_WasDown = false;
	mbP1_Crouch_WasDown = false;
	mbP1_Run_WasDown = false;
	mbPossessKeyWasDown = false;

	for(int i=0; i<(int)(sizeof(mvMorphKeyWasDown)/sizeof(mvMorphKeyWasDown[0])); ++i)
		mvMorphKeyWasDown[i] = false;

	mbP2Raw_Jump_WasDown = false;
	mbP2Raw_Run_WasDown = false;

	mbP1ActionFilterActive = false;
	mvP1ActionDown.assign(eLuxAction_LastEnum, false);
	mvP1ActionBecame.assign(eLuxAction_LastEnum, false);
	mvP1ActionReleased.assign(eLuxAction_LastEnum, false);
	mbP2Raw_Crouch_WasDown = false;
	mbP2Raw_Lantern_WasDown = false;
	mbP2Raw_Interact_WasDown = false;
	mbP2Raw_Attack_WasDown = false;
	mbP2Raw_Inventory_WasDown = false;
	mbP2Raw_Journal_WasDown = false;
	mbP2Raw_QuestLog_WasDown = false;
	mbP2Raw_RecentText_WasDown = false;
	mbP2Raw_NoteAdvance_WasDown = false;

	mbP2Fx_Primary_WasDown = false;
	mbP2Fx_Secondary_WasDown = false;

	mbP2Raw_GuiLeft_WasDown = false;
	mbP2Raw_GuiRight_WasDown = false;
	mbP2Raw_GuiExit_WasDown = false;
	mbP2Raw_GuiInventory_WasDown = false;
	mbP2Raw_GuiJournal_WasDown = false;

	mvCoopCursorPos = 0;
	mvCoopCursorRel = 0;
	mbCoopCursorValid = false;
	mvCoopCursorPosP2 = 0;
	mbCoopCursorP2Valid = false;
	mvFrameMouseRel = 0;
	mfP2LastGuiClickTime = -1.0;

#ifdef USE_GAMEPAD
	mbP2_A_WasDown = false;
	mbP2_B_WasDown = false;
	mbP2_X_WasDown = false;
	mbP2_LS_WasDown = false;
	mbP2_RS_WasDown = false;
	mbP2_Run_WasDown = false;
	mbP2_GunTrigger_WasDown = false;
	mbP2_Y_WasDown = false;
	mbP2_Back_WasDown = false;
	mbP2_InvBack_WasDown = false;
	mbP2_JournalBack_WasDown = false;
	mbP1GameInputRan = false;
	mbP2GameInputRan = false;
	for(int i=0; i<16; ++i) mbPadUIHeld[i] = false;
	mbP2_BothSticks_WasDown = false;
#endif
}

//-----------------------------------------------------------------------

cLuxInputHandler::~cLuxInputHandler()
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxInputHandler::LoadUserConfig()
{
	mbInvertMouse = gpBase->mpUserConfig->GetBool("Input", "InvertMouse", false);
	mbSmoothMouse = gpBase->mpUserConfig->GetBool("Input", "SmoothMouse", true);

	mfMouseSensitivity = gpBase->mpUserConfig->GetFloat("Input", "MouseSensitivity", 1.0f);

#ifdef USE_GAMEPAD
	mfGamepadWalkSensitivity = gpBase->mpUserConfig->GetFloat("Input", "GamepadWalkSensitivity", 1.0f);
	mfGamepadLookSensitivity = gpBase->mpUserConfig->GetFloat("Input", "GamepadLookSensitivity", 1.5f);

	mbGamepadLookInvert = gpBase->mpUserConfig->GetBool("Input", "InvertGamepadLook", false);
#endif


	tString sSep = ".";
	///////////////////////////////////////////////////////////
	// Load user key config, clear configurable keys first
	for(int i=0;gvLuxActions[i].msName!="";++i)
	{
		cLuxAction* pLuxAction = &gvLuxActions[i];
		if(pLuxAction->mbConfigurable==false)
			continue;

		cAction* pAction = mpInput->GetAction(pLuxAction->mlId);
		pAction->ClearSubActions();
		
		////////////////////////////////////////////////////////////////
		// Get user key for primary and secondary
		bool bHasUserDefinedInputs = false;

		for(size_t j=0; gvLuxInputPos[j]!=""; ++j)
		{
			tString sInput = gpBase->mpUserKeyConfig->GetString(pAction->GetName(), gvLuxInputPos[j], "");
			if(sInput.empty())
				continue;

			bHasUserDefinedInputs = CreateSubActionFromInputString(pAction, sInput) || 
									bHasUserDefinedInputs;
		}

		// If no valid inputs were loaded, load default
		if(bHasUserDefinedInputs==false)
		{
			tLuxInputVec vDefaultInputs = GetDefaultInputsByActionId(pLuxAction->mlId);

			for(size_t i=0; i<vDefaultInputs.size(); ++i)
			{
				cLuxInput* pDefaultInput = vDefaultInputs[i];
				tStringVec vInputParts;
				cString::GetStringVec(pDefaultInput->msInputType, vInputParts, &sSep);
				CreateSubAction(pAction, vInputParts, pDefaultInput->mlValue);

#if 0 && defined(__APPLE__)
				// Heinous Kludge to get a default Mac keyboard shortcut without relying on different config files.
				if (pDefaultInput->mlActionId == eLuxAction_Attack) {
					CreateSubAction(pAction, "Keyboard", eKey_LeftMeta);
				}
#endif
			}
		}
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::SaveUserConfig()
{
	gpBase->mpUserConfig->SetBool("Input", "InvertMouse", mbInvertMouse);
	gpBase->mpUserConfig->SetBool("Input", "SmoothMouse", mbSmoothMouse);

	gpBase->mpUserConfig->SetFloat("Input", "MouseSensitivity", mfMouseSensitivity);

#ifdef USE_GAMEPAD
	gpBase->mpUserConfig->SetBool("Input", "InvertGamepadLook", mbGamepadLookInvert);
	
	gpBase->mpUserConfig->SetFloat("Input", "GamepadWalkSensitivity", mfGamepadWalkSensitivity);
	gpBase->mpUserConfig->SetFloat("Input", "GamepadLookSensitivity", mfGamepadLookSensitivity);
#endif

	//////////////////////////////////////////////////
	// Save key config
	tStringVec vInputStringFields;
	for(int i=0;gvLuxInputPos[i]!="";++i)
		vInputStringFields.push_back(gvLuxInputPos[i]);

	// Go through all actions and save configurable ones
	for(int i=0;gvLuxActions[i].msName!="";++i)
	{
		cLuxAction* pLuxAction = &gvLuxActions[i];
		if(pLuxAction->mbConfigurable==false)
			continue;

		cAction* pAction = mpInput->GetAction(pLuxAction->mlId);
		size_t j=0;
		////////////////////////////////////////////
		// Save sub actions in action
		for(;j<pAction->GetSubActionNum() && j<vInputStringFields.size();++j)
		{
			iSubAction* pSubAction = pAction->GetSubAction(j);
			const tString& sInputPos = vInputStringFields[j];
			
			tString sInputValue = pSubAction->GetInputType() + gsLuxInputStringSeparator + pSubAction->GetInputName();
			
			gpBase->mpUserKeyConfig->SetString(pAction->GetName(), sInputPos, sInputValue);
		}
		///////////////////////////////////////////////////////////////////
		// Fill up with empty sub actions to fill up
		for(;j<vInputStringFields.size();++j)
			gpBase->mpUserKeyConfig->SetString(pAction->GetName(), vInputStringFields[j], "");
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::OnStart()
{
	mpPlayer = gpBase->mpPlayer;
}

//-----------------------------------------------------------------------

void cLuxInputHandler::Update(float afTimeStep)
{
	///////////////////////////////////
	// Update input for current state
	mbP1GameInputRan = false;
	mbP2GameInputRan = false;

	//////////////////////////////////////////////////////////////////////
	// Take the mouse delta once, here, before anything else can.
	//
	// cMouseSDL::GetRelPosition() empties itself on read -- it is not a getter,
	// it is a take. Everything below asks GetFrameMouseRel() instead, so the menu
	// cursor and the free player's mouselook see the same motion in the same
	// frame rather than one of them silently being handed a zero.
	mvFrameMouseRel = mpInput->GetMouse()->GetRelPosition();

	//////////////////////////////////////////////////////////////////////
	// PLAYER 2'S DEVICE OUT OF PLAYER 1'S ACTIONS -- EVERY TICK, NOT JUST IN GAME.
	//
	// This used to live in UpdateGamePlayerInput, which only runs in the Game
	// state. Two things followed, and both were reported:
	//
	//   In a menu it never ran, so mvP1ActionBecame stayed frozen at whatever the
	//   last gameplay frame left -- all false. P1BecameTriggerd(LeftClick) could
	//   then never fire, and Player 1 could move the cursor around their own
	//   journal without being able to click a single thing in it.
	//
	//   Worse on the way out: mbP1ActionFilterActive was ALSO left frozen, at
	//   true. Quit to the main menu and every action still came from a table
	//   nothing was updating any more, so the main menu was unclickable until the
	//   game was restarted.
	//
	// Once a tick, before anything reads an action. Still a no-op unless Player 2
	// owns a keyboard of their own.
	UpdateP1ActionFilter();

	//Which mouse is Player 2's. Costs nothing once it has decided, and nothing at
	//all unless Player 2 is on a keyboard of their own.
	UpdateP2MousePairing();

	UpdateGlobalInput();

	switch(mState)
	{
	//Game
	case eLuxInputState_Game: UpdateGameInput(); break;
	//Main Menu
	case eLuxInputState_MainMenu: UpdateMainMenuInput(); break;
	//Pre Menu
	case eLuxInputState_PreMenu: UpdatePreMenuInput(); break;
	//Inventory
	case eLuxInputState_Inventory: UpdateInventoryInput(); break;
	//Journal
	case eLuxInputState_Journal: UpdateJournalInput(); break;
	//Debug
	case eLuxInputState_Debug: UpdateDebugInput(); break;
	//Credits
	case eLuxInputState_Credits: UpdateCreditsInput(); break;
	//Demo End
	case eLuxInputState_DemoEnd: UpdateDemoEndInput(); break;
	//Load Screen
	case eLuxInputState_LoadScreen: UpdateLoadScreenInput(); break;
	}

	//Coop: whoever is NOT in the menu keeps playing. Outside the switch on
	//purpose -- the menus pass through eLuxInputState_Null while fading, and that
	//hits no case at all, so hanging this off a case froze the free player for as
	//long as a note was up. It no-ops whenever no menu is open.
	UpdateCoopFreeRoamInput();

	//////////////////////////////////////////////////////////////////////
	// ...and the rest of them, not just their input.
	//
	// cLuxPlayer is a "Default" container module. The moment a menu opens the
	// container is "Journal" or "Inventory" and cLuxPlayer::Update stops being
	// called for EITHER player -- Player 2 is updated from inside Player 1's
	// pass, so one container switch takes both. Input reached the free player and
	// went nowhere, because the move state that turns input into movement was not
	// running. That is "I cannot look around while they have their journal open",
	// and it was never an input problem at all.
	cLuxPlayer *pFree = GetCoopFreeRoamPlayer();
	if(pFree) pFree->Update(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxInputHandler::PostUpdate(float afTimeStep)
{
	cLuxPlayer *pFree = GetCoopFreeRoamPlayer();
	if(pFree) pFree->PostUpdate(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxInputHandler::OnDraw(float afFrameTime)
{
	//Their HUD. cLuxPlayer::OnDraw is gone with the rest of the container, which
	//is why the grab and lever icons disappeared off doors the free player was
	//looking straight at.
	cLuxPlayer *pFree = GetCoopFreeRoamPlayer();
	if(pFree) pFree->OnDraw(afFrameTime);
}

//-----------------------------------------------------------------------

cLuxPlayer* cLuxInputHandler::GetCoopFreeRoamPlayer()
{
	if(gpBase->mpMapHandler->GetCoopMode()==false) return NULL;
	if(gpBase->mpPlayer2 == NULL) return NULL;

	//A note they are both reading is a real pause. Neither of them is free.
	if(CoopMenuIsShared()) return NULL;

	cLuxPlayer *pOwner = GetCoopMenuOwner();
	if(pOwner == NULL) return NULL;	//no menu open; the container is ticking them

	return pOwner->IsPlayer2() ? gpBase->mpPlayer : gpBase->mpPlayer2;
}

//-----------------------------------------------------------------------

void cLuxInputHandler::Reset()
{

}

//-----------------------------------------------------------------------

void cLuxInputHandler::OnPostRender(float afFrameTime)
{
	//Turn of logging so it only happens one frame!
	gpBase->mpMapHandler->GetViewport()->GetRenderSettings()->mbLog = false;
}

//-----------------------------------------------------------------------

tWString gsInvalidActionString = _W("InvalidAction");

tWString cLuxInputHandler::GetInputName(const tString& asActionName)
{
	cAction *pAction = mpInput->GetAction(asActionName);
	if(pAction == NULL) return gsInvalidActionString;

    iSubAction *pSubAction = pAction->GetSubAction(0);

	return cString::To16Char(pSubAction->GetInputName());
}

//-----------------------------------------------------------------------

void cLuxInputHandler::ChangeState(eLuxInputState aState)
{
	mState = aState;


	//This rests all actions so changing states does not give unwanted clicks!
	mpInput->ResetActionsToCurrentState();

#ifdef USE_GAMEPAD
	// Do the same for Player 2's hand-rolled gamepad edge detection, so a button
	// still held across the transition (e.g. the Back button used to open/close
	// the inventory while the world is paused) doesn't misfire on the first
	// frame of the new state.
	ResetPlayer2InputState();
#endif

	//Same job for Player 2's second keyboard and mouse: seed the menu-side edges
	//from the live device, so the Tab that opened a bag is not read as a fresh
	//Tab closing it on the very next frame.
	ResetPlayer2RawGuiState();

	//Reset some stuff
	mlstSmoothMousePos.clear();	//Every state needs new smoothing!

	mvLastAbsMousePos = mpInput->GetMouse()->GetAbsPosition();
}

//-----------------------------------------------------------------------

void cLuxInputHandler::SetMouseSensitivity(float afX)
{
	if(mfMouseSensitivity==afX)
		return;

	mfMouseSensitivity = afX;
}

//-----------------------------------------------------------------------

#ifdef USE_GAMEPAD
void cLuxInputHandler::SetGamepadLookSensitivity(float afX)
{
	if(mfGamepadLookSensitivity==afX)
		return;

	mfGamepadLookSensitivity = afX;
}
#endif

//-----------------------------------------------------------------------

cLuxAction* cLuxInputHandler::GetActionByName(const tString& asName)
{
	for(int i=0; gvLuxActions[i].msName!=""; ++i)
	{
		cLuxAction* pAction = &gvLuxActions[i];

		if(pAction->msName == asName) return pAction;
	}

	return NULL;
}

//-----------------------------------------------------------------------

cLuxAction* cLuxInputHandler::GetActionById(int alId)
{
	for(int i=0; gvLuxActions[i].msName!=""; ++i)
	{
		cLuxAction* pAction = &gvLuxActions[i];

		if(pAction->mlId==alId)
			return pAction;
	}

	return NULL;
}

//-----------------------------------------------------------------------

tLuxActionVec cLuxInputHandler::GetActionsByCategory(eLuxActionCategory aCat)
{
	bool bAddAll = (aCat==eLuxActionCategory_LastEnum);

	tLuxActionVec vActions;
	for(int i=0; gvLuxActions[i].msName!=""; ++i)
	{
		cLuxAction* pAction = &gvLuxActions[i];

		if(bAddAll || pAction->mCat==aCat)
			vActions.push_back(pAction);
	}

	return vActions;
}

//-----------------------------------------------------------------------

tLuxInputVec cLuxInputHandler::GetDefaultInputsByActionId(int alId)
{
	tLuxInputVec vInputs;
	for(int i=0; gvLuxInputs[i].msInputType!=""; ++i)
	{
		cLuxInput* pInput = &gvLuxInputs[i];

		if(pInput->mlActionId==alId)
			vInputs.push_back(pInput);
	}

	return vInputs;
}

//-----------------------------------------------------------------------

void cLuxInputHandler::ResetSmoothMousePos()
{
	mlstSmoothMousePos.clear();
}

//-----------------------------------------------------------------------

cVector2f cLuxInputHandler::GetSmoothMousePos(const cVector2f& avRelPosMouse)
{
	mlstSmoothMousePos.push_front(avRelPosMouse);
	if((int)mlstSmoothMousePos.size() > mlMaxSmoothMousePos) mlstSmoothMousePos.pop_back();

	float fWeight = 1.0f;
	float fWeightSum =0;
	cVector2f vPosSum=0;
	for(tVector2fListIt it = mlstSmoothMousePos.begin(); it != mlstSmoothMousePos.end(); ++it)
	{
		vPosSum += *it * fWeight;
		fWeightSum += fWeight;
		fWeight *= mfPrevSmoothMousePosMul;	//decrease influence of next pos.
	}

	return vPosSum / fWeightSum;
}

//-----------------------------------------------------------------------

#ifdef USE_GAMEPAD
bool cLuxInputHandler::IsGamepadPresent()
{
	return mpPad!=NULL;
}

//-----------------------------------------------------------------------

void cLuxInputHandler::ResetPlayer2InputState()
{
	// Seed every edge tracker from the CURRENT physical button state. A button
	// still held when the state changes is treated as "already down", so only a
	// fresh press (release + press) after the transition counts as a new edge.
	// This stops the Back button used to open/close the inventory from instantly
	// re-triggering, and stops held buttons from misfiring when the (paused)
	// menu closes.
	if(mpPad)
	{
#if USE_SDL2
		mbP2_A_WasDown       = mpPad->ButtonIsDown(eGamepadButton_A);
		mbP2_B_WasDown       = mpPad->ButtonIsDown(eGamepadButton_B);
		mbP2_X_WasDown       = mpPad->ButtonIsDown(eGamepadButton_X);
		mbP2_LS_WasDown      = mpPad->ButtonIsDown(eGamepadButton_LeftShoulder);
		mbP2_RS_WasDown      = mpPad->ButtonIsDown(eGamepadButton_RightShoulder);
		mbP2_Run_WasDown     = mpPad->GetAxisValue(eGamepadAxis_LeftTrigger) > 0.5f;
		mbP2_GunTrigger_WasDown = mpPad->GetAxisValue(eGamepadAxis_RightTrigger) > 0.5f;
		mbP2_Y_WasDown       = mpPad->ButtonIsDown(eGamepadButton_Y);
		mbP2_Back_WasDown    = mpPad->ButtonIsDown(eGamepadButton_Back);
		mbP2_InvBack_WasDown = mpPad->ButtonIsDown(eGamepadButton_Back);
		mbP2_BothSticks_WasDown = mpPad->ButtonIsDown(eGamepadButton_LeftStick) && mpPad->ButtonIsDown(eGamepadButton_RightStick);

		//UpdateGamepadUIInput bails the moment the pad stops owning a menu, so a
		//flag set while P2 held a UI button never reaches its release. It survived
		//the menu closing and fired one phantom release into whatever had focus the
		//next time the pad owned a menu.
		for(int i=0; i<16; ++i) mbPadUIHeld[i] = false;
#else
		mbP2_A_WasDown       = mpPad->ButtonIsDown(eGamepadButton_0);
		mbP2_B_WasDown       = mpPad->ButtonIsDown(eGamepadButton_1);
		mbP2_X_WasDown       = mpPad->ButtonIsDown(eGamepadButton_2);
		mbP2_LS_WasDown      = mpPad->ButtonIsDown(eGamepadButton_4);
		mbP2_RS_WasDown      = mpPad->ButtonIsDown(eGamepadButton_5);
		mbP2_Run_WasDown     = false;
		mbP2_GunTrigger_WasDown = false;
		mbP2_Y_WasDown       = mpPad->ButtonIsDown(eGamepadButton_3);
		mbP2_Back_WasDown    = mpPad->ButtonIsDown(eGamepadButton_6);
		mbP2_InvBack_WasDown = mpPad->ButtonIsDown(eGamepadButton_6);
		mbP2_BothSticks_WasDown = mpPad->ButtonIsDown(eGamepadButton_8) && mpPad->ButtonIsDown(eGamepadButton_9);
#endif
	}
	else
	{
		mbP2_A_WasDown = false;
		mbP2_B_WasDown = false;
		mbP2_X_WasDown = false;
		mbP2_LS_WasDown = false;
		mbP2_RS_WasDown = false;
		mbP2_Run_WasDown = false;
		mbP2_GunTrigger_WasDown = false;
		mbP2_Y_WasDown = false;
		mbP2_Back_WasDown = false;
		mbP2_InvBack_WasDown = false;
		mbP2_BothSticks_WasDown = false;

		for(int i=0; i<16; ++i) mbPadUIHeld[i] = false;
	}
}
#endif

//-----------------------------------------------------------------------

#ifdef USE_GAMEPAD
void cLuxInputHandler::AppDeviceWasPlugged()
{
	SetUpGamepad();
}
#endif

#ifdef USE_GAMEPAD
void cLuxInputHandler::AppDeviceWasRemoved()
{
	SetUpGamepad();
}
#endif

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cVector2l cLuxInputHandler::GetGuiMouseAbsPos()
{
	//The owner-driven pointer whenever there is one; SDL's otherwise. See
	//UpdateCoopMenuCursor for why SDL's own position cannot be used once a second
	//mouse exists -- it is the sum of both of them.
	cVector2l vAbs = mbCoopCursorValid ? mvCoopCursorPos : mpInput->GetMouse()->GetAbsPosition();

	//////////////////////////////////////////////////////////////////
	// In coop a menu renders inside the opening player's viewport rect, but
	// cGui::SendMousePos divides the absolute position by the FULL screen size
	// before mapping to virtual GUI coordinates. Remap the pointer to be
	// viewport-relative and scaled back up to screen size, so the standard
	// mapping yields coordinates that match where the GUI is drawn.
	//
	// The journal is in here as well as the inventory now. It used to be
	// full-screen-only and did not need it; a journal that belongs to one player
	// is drawn on their half and needs exactly the same remap.
	if (gpBase->mpMapHandler->GetCoopMode())
	{
		cViewport *pVp = GetCoopMenuViewport();
		cVector2f vScreen = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();

		//////////////////////////////////////////////////////////////////
		// POSITION counts as well as size, and leaving it out is why Player 2
		// had no cursor.
		//
		// This used to remap only when the viewport's SIZE differed from the
		// screen. In dual monitor Player 2's half is a whole monitor: same size
		// as the screen, at x = 2560. Same size, so no remap -- but the pointer
		// is still being fed an absolute position starting at 2560, and
		// cGui::SendMousePos divides by the screen width. 2560/2560 is 1.0, the
		// far right edge, and everything further right is past it. Player 2's
		// cursor was not missing, it was parked permanently off the end of the
		// GUI.
		//
		// Player 1's half is at x = 0, so the offset was zero and theirs worked.
		// That is the entire difference between the two.
		const bool bOffset = (pVp && (pVp->GetPosition().x != 0 || pVp->GetPosition().y != 0));
		const bool bScaled = (pVp && ((float)pVp->GetSize().x != vScreen.x ||
										(float)pVp->GetSize().y != vScreen.y));

		if (pVp && pVp->GetSize().x > 0 && pVp->GetSize().y > 0 && (bOffset || bScaled))
		{
			float fX = (float)(vAbs.x - pVp->GetPosition().x) * (vScreen.x / (float)pVp->GetSize().x);
			float fY = (float)(vAbs.y - pVp->GetPosition().y) * (vScreen.y / (float)pVp->GetSize().y);
			vAbs = cVector2l((int)fX, (int)fY);
		}
	}

	return vAbs;
}

//-----------------------------------------------------------------------

cVector2l cLuxInputHandler::GetGuiMouseRelPos()
{
	cVector2l vRel = mbCoopCursorValid ? mvCoopCursorRel : GetFrameMouseRel();

	// Scale relative motion to match the remapped absolute position (see
	// GetGuiMouseAbsPos).
	if (gpBase->mpMapHandler->GetCoopMode())
	{
		cViewport *pVp = GetCoopMenuViewport();
		cVector2f vScreen = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();
		if (pVp && pVp->GetSize().x > 0 && pVp->GetSize().y > 0 &&
			((float)pVp->GetSize().x != vScreen.x || (float)pVp->GetSize().y != vScreen.y))
		{
			vRel = cVector2l(	(int)((float)vRel.x * (vScreen.x / (float)pVp->GetSize().x)),
								(int)((float)vRel.y * (vScreen.y / (float)pVp->GetSize().y)));
		}
	}

	return vRel;
}

//-----------------------------------------------------------------------

cVector2l cLuxInputHandler::GetGuiMouseAbsPosP2()
{
	//Only ever asked for while a SHARED note is up. That note's viewport is the
	//journal's, which is the whole screen at the origin -- not one player's half
	//-- so the offset/scale remap GetGuiMouseAbsPos does for a per-player menu is
	//the identity here and there is nothing to apply. Screen pixels straight
	//through, exactly as Player 1's takes on the same path.
	return mvCoopCursorPosP2;
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateGlobalInput()
{
	/////////////////
	// ScreenShot
	if(mpInput->BecameTriggerd(eLuxAction_ScreenShot))
	{
		tWString sFileName = _W("");
		tWString sBaseName = _W("Screen_");
#ifndef _WIN32
		tWString sScreenShotDir = cPlatform::GetSystemSpecialPath(eSystemPath_Personal);
		if (cPlatform::FolderExists(cString::AddSlashAtEndW(sScreenShotDir) + _W("Desktop"))) {
			sScreenShotDir = cString::AddSlashAtEndW(sScreenShotDir) + _W("Desktop");
		}
		sBaseName = cString::AddSlashAtEndW(sScreenShotDir) + _W("Amnesia_");
#endif
		int lCount = 0;

		do{
			sFileName = sBaseName+cString::To16Char("Screenshot")+_W("_");
			sFileName += cString::ToStringW(lCount,3);
			/*
			if(lCount >= 100)		sFileName += _W("")+cString::ToStringW(lCount);
			else if(lCount >= 10)	sFileName += _W("0")+cString::ToStringW(lCount);
			else					sFileName += _W("00")+cString::ToStringW(lCount);
			*/

			sFileName += _W(".")+cString::To16Char(gpBase->mpConfigHandler->msScreenShotExt);
			++lCount;

		}
		while(cPlatform::FileExists(sFileName));
		
		cEngine *pEngine = gpBase->mpEngine;
		
		cBitmap *pBmp = pEngine->GetGraphics()->GetLowLevel()->CopyFrameBufferToBitmap();
		pEngine->GetResources()->GetBitmapLoaderHandler()->SaveBitmap(pBmp,sFileName,0);
		hplDelete(pBmp);
	}

	/////////////////
	// Debug output
	//
	//One press dumps every draw call of one frame -- about 3,500 lines of
	//"Setting model matrix" -- which buries everything useful in hpl.log. P is an
	//easy key to hit by accident, so it now needs the debug menu enabled, like the
	//other debug-only keys in this function. Set LoadDebugMenu in main_settings.cfg
	//to get it back.
	if(gpBase->mpConfigHandler->mbLoadDebugMenu &&
		mpInput->BecameTriggerd(eLuxAction_PrintInfo))
	{
		gpBase->mpMapHandler->GetViewport()->GetRenderSettings()->mbLog = true;
	}

	/////////////////
	// The menu pointer, before anything is sent anywhere.
	UpdateCoopMenuCursor();

	/////////////////
	// Send to GUI
	// Coop: not into a menu the pad owns -- P1 is out there playing.
	if(CurrentStateSendsInputToGui() && CoopMenuAcceptsKeyboard())
	{
		cGui *pGui = gpBase->mpEngine->GetGui();

		//Key presses
		while(mpInput->GetKeyboard()->KeyIsPressed())
		{
			//Two keyboards, one SDL queue, and no device on the event -- so with a
			//second keyboard in play there is no way to know which player typed
			//this. Drained and dropped rather than delivered: nothing in co-op
			//needs typing, and the menus are driven by the actions below, which DO
			//know the device. Single player and pad co-op are untouched.
			if(mbP1ActionFilterActive)	mpInput->GetKeyboard()->GetKey();
			else						pGui->SendKeyPress(mpInput->GetKeyboard()->GetKey());
		}

		//Mouse movement
		pGui->SendMousePos(GetGuiMouseAbsPos(), GetGuiMouseRelPos());

		
		//Mouse click.
		//
		//GuiActionBecame, not the raw action: Player 2 is out in the world with a
		//live mouse and SDL merges both of them, so an unfiltered read let P2's
		//clicking press the buttons in P1's open bag. It passes straight through
		//in single player, in pad co-op, and for a note they are both reading.
		if(GuiActionBecame(eLuxAction_LeftClick))				pGui->SendMouseClickDown(eGuiMouseButton_Left);
		if(GuiActionWas(eLuxAction_LeftClick))					pGui->SendMouseClickUp(eGuiMouseButton_Left);
		if(mpInput->DoubleTriggerd(eLuxAction_LeftClick, 0.3f))	pGui->SendMouseDoubleClick(eGuiMouseButton_Left);

		if(GuiActionBecame(eLuxAction_MiddleClick))		
			pGui->SendMouseClickDown(eGuiMouseButton_Middle);
		if(GuiActionWas(eLuxAction_MiddleClick))					pGui->SendMouseClickUp(eGuiMouseButton_Middle);
		if(mpInput->DoubleTriggerd(eLuxAction_MiddleClick, 0.3f))	pGui->SendMouseDoubleClick(eGuiMouseButton_Middle);

		if(GuiActionBecame(eLuxAction_RightClick))				pGui->SendMouseClickDown(eGuiMouseButton_Right);
		if(GuiActionWas(eLuxAction_RightClick))					pGui->SendMouseClickUp(eGuiMouseButton_Right);
		if(mpInput->DoubleTriggerd(eLuxAction_RightClick, 0.3f))	pGui->SendMouseDoubleClick(eGuiMouseButton_Right);

		if(mpInput->BecameTriggerd(eLuxAction_ScrollUp))		pGui->SendMouseClickDown(eGuiMouseButton_WheelUp);
		if(mpInput->WasTriggerd(eLuxAction_ScrollUp))		pGui->SendMouseClickUp(eGuiMouseButton_WheelUp);

		if(mpInput->BecameTriggerd(eLuxAction_ScrollDown))		pGui->SendMouseClickDown(eGuiMouseButton_WheelDown);
		if(mpInput->WasTriggerd(eLuxAction_ScrollDown))		pGui->SendMouseClickUp(eGuiMouseButton_WheelDown);

		if(mpInput->BecameTriggerd(eLuxAction_MouseButton6Click))		pGui->SendMouseClickDown(eGuiMouseButton_Button6);
		if(mpInput->WasTriggerd(eLuxAction_MouseButton6Click))		pGui->SendMouseClickUp(eGuiMouseButton_Button6);
		if(mpInput->DoubleTriggerd(eLuxAction_MouseButton6Click, 0.3f))	pGui->SendMouseDoubleClick(eGuiMouseButton_Button6);

		if(mpInput->BecameTriggerd(eLuxAction_MouseButton7Click))		pGui->SendMouseClickDown(eGuiMouseButton_Button7);
		if(mpInput->WasTriggerd(eLuxAction_MouseButton7Click))		pGui->SendMouseClickUp(eGuiMouseButton_Button7);
		if(mpInput->DoubleTriggerd(eLuxAction_MouseButton7Click, 0.3f))	pGui->SendMouseDoubleClick(eGuiMouseButton_Button7);
		
		if(mpInput->BecameTriggerd(eLuxAction_MouseButton8Click))		pGui->SendMouseClickDown(eGuiMouseButton_Button8);
		if(mpInput->WasTriggerd(eLuxAction_MouseButton8Click))		pGui->SendMouseClickUp(eGuiMouseButton_Button8);
		if(mpInput->DoubleTriggerd(eLuxAction_MouseButton8Click, 0.3f))	pGui->SendMouseDoubleClick(eGuiMouseButton_Button8);

		if(mpInput->BecameTriggerd(eLuxAction_MouseButton9Click))		pGui->SendMouseClickDown(eGuiMouseButton_Button9);
		if(mpInput->WasTriggerd(eLuxAction_MouseButton9Click))		pGui->SendMouseClickUp(eGuiMouseButton_Button9);
		if(mpInput->DoubleTriggerd(eLuxAction_MouseButton9Click, 0.3f))	pGui->SendMouseDoubleClick(eGuiMouseButton_Button9);

	}
	else if(CurrentStateSendsInputToGui())
	{
		//The keyboard is shut out of this menu, but its press queue still fills up.
		//Drained so it cannot arrive all at once the moment the menu closes.
		while(mpInput->GetKeyboard()->KeyIsPressed()) mpInput->GetKeyboard()->GetKey();
	}

	/////////////////
	// Player 2's own keyboard and mouse, when the open menu is theirs.
	//
	// Outside the block above on purpose: that one is gated on the SDL keyboard,
	// which is Player 1's. Each device is allowed or refused on its own -- exactly
	// like the pad below. The pointer is already theirs (UpdateCoopMenuCursor);
	// this adds the clicks.
	//
	// Not for a SHARED note: there the SDL feed above already carries both mice,
	// and adding this would deliver every one of Player 2's clicks twice.
	if(CurrentStateSendsInputToGui() && CoopMenuIsShared() && CoopMenuAcceptsP2Raw())
	{
		//////////////////////////////////////////////////////////////////
		// A SHARED NOTE: Player 2 clicks at PLAYER 2'S pointer.
		//
		// The set has one cursor and it is Player 1's -- it was handed Player
		// 1's position at the top of this function and it keeps it. So Player
		// 2's position goes in only for the instant their click is delivered,
		// and Player 1's goes straight back afterwards.
		//
		// Only on a frame where a button of theirs actually changed state.
		// Sending it every frame would drag the set's hover and focus back and
		// forth between the two of them and make both halves flicker.
		cGui *pGui = gpBase->mpEngine->GetGui();
		const bool bEdge = P2RawGuiHasButtonEdge();

		if(bEdge) pGui->SendMousePos(GetGuiMouseAbsPosP2(), cVector2l(0,0));

		SendP2RawInputToGui();

		if(bEdge) pGui->SendMousePos(GetGuiMouseAbsPos(), cVector2l(0,0));
	}
	else if(CurrentStateSendsInputToGui() && CoopMenuIsShared()==false && CoopMenuAcceptsP2Raw())
	{
		//////////////////////////////////////////////////////////////////
		// The POSITION, not just the buttons.
		//
		// cGui::SendMousePos was only ever called inside the keyboard block
		// above, and that block is shut precisely when Player 2 owns the menu --
		// so the pointer was computed for them every frame by
		// UpdateCoopMenuCursor and then never handed to the GUI. Player 2 had a
		// cursor that was drawn (OnEnterContainer switches it on for them) and
		// pinned to wherever it was seeded, which is "no cursor" to play with.
		gpBase->mpEngine->GetGui()->SendMousePos(GetGuiMouseAbsPos(), GetGuiMouseRelPos());

		SendP2RawInputToGui();
	}

#ifdef USE_GAMEPAD
	//The pad gets its OWN gate, outside the keyboard block. It used to live
	//inside it, so shutting the keyboard out of a P2-owned menu shut the pad out
	//too and left P2 unable to touch their own bag. Each device is allowed or
	//refused on its own; UpdateGamepadUIInput checks CoopMenuAcceptsGamepad().
	mbGamepadUIInput = false;
	if(CurrentStateSendsInputToGui())
		mbGamepadUIInput = UpdateGamepadUIInput();
#endif
}

#ifdef USE_GAMEPAD
bool cLuxInputHandler::UpdateGamepadUIInput()
{
	//Coop: the pad is P2's. If the open menu is P1's, P2 is still out in the
	//world and every stick flick would otherwise walk P1's cursor around.
	if(CoopMenuAcceptsGamepad()==false) return false;

	bool bRet = false;
	cGui* pGui = gpBase->mpEngine->GetGui();

	//Key presses
	if(IsGamepadPresent())
	{
		while(mpPad->HasInputUpdates())
		{
			pGui->SendGamepadInput(mpPad->GetInputUpdate());
		}
	}

	int vActionIDs[] = 
	{
		eLuxAction_UIArrowUp,
		eLuxAction_UIArrowRight,
		eLuxAction_UIArrowDown,
		eLuxAction_UIArrowLeft,

		-1,

		eLuxAction_UIPrimary,
		eLuxAction_UISecondary,
		eLuxAction_UIPrevPage,
		eLuxAction_UINextPage,
		eLuxAction_Interact,
		eLuxAction_UIDelete,
		eLuxAction_UIClear,

		-1
	};

	int vInputIDs[] = 
	{
		eUIArrow_Up,
		eUIArrow_Right,
		eUIArrow_Down,
		eUIArrow_Left,

		-1,

		eUIButton_Primary,
		eUIButton_Secondary,
		eUIButton_PrevPage,
		eUIButton_NextPage,
		eUIButton_Interact,
		eUIButton_Delete,
		eUIButton_Clear,

		-1
	};

	//Coop: several of these actions are shared with gameplay -- Interact is the
	//obvious one, bound to the pad's right shoulder AND to P1's left mouse button.
	//Reading BecameTriggerd for an action the pad is not touching CLEARS it, and
	//this runs before P1's gameplay input, so it stole every grab and click P1
	//made while P2 had a menu open. Only touch an action the PAD is actually
	//firing, and only release what we pressed.
	bool bCoopFilter = gpBase->mpMapHandler->GetCoopMode();

	int i=0;
	for(; vActionIDs[i]!=-1; ++i)
	{
		int lAction = vActionIDs[i];
		int lInput = vInputIDs[i];

		bool bPadSource = (bCoopFilter==false) || SubActionTriggeredForDevice(lAction, true);

		if(bPadSource && mpInput->BecameTriggerd(lAction))
		{
			pGui->SendUIArrowPress((eUIArrow)lInput);
			mbPadUIHeld[i] = true;
			bRet = true;
		}
		if(mbPadUIHeld[i] && mpInput->WasTriggerd(lAction))
		{
			pGui->SendUIArrowRelease((eUIArrow)lInput);
			mbPadUIHeld[i] = false;
			bRet = true;
		}
	}

	++i;

	for(; vActionIDs[i]!=-1; ++i)
	{
		int lAction = vActionIDs[i];
		int lInput = vInputIDs[i];

		bool bPadSource = (bCoopFilter==false) || SubActionTriggeredForDevice(lAction, true);

		if(bPadSource && mpInput->BecameTriggerd(lAction))
		{
			pGui->SendUIButtonPress((eUIButton)lInput);
			mbPadUIHeld[i] = true;
			bRet = true;
		}
		if(mbPadUIHeld[i] && mpInput->WasTriggerd(lAction))
		{
			pGui->SendUIButtonRelease((eUIButton)lInput);
			mbPadUIHeld[i] = false;
			bRet = true;
		}
		if(bPadSource && mpInput->DoubleTriggerd(lAction, 0.3f))
		{
			pGui->SendUIButtonDoublePress((eUIButton)lInput);
			bRet = true;
		}
	}

	return bRet;
}
#endif

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateGameInput()
{
	/////////////////
	// Debug: co-op player teleports on F1/F2. While the Player Teleport
	// debug option is ON (default) and coop is running, these OVERRIDE the
	// old debug-window/reload-map bindings on the same keys — turn the
	// option off in the debug menu to get those back.
	bool bCoopTeleport = ImGuiDebugMenu::GetPlayerTeleport() &&
						 gpBase->mpMapHandler->GetCoopMode() && gpBase->mpPlayer2 != NULL;

	if(mpInput->BecameTriggerd(eLuxAction_OpenDebug))
	{
		if(bCoopTeleport)
			gpBase->mpMapHandler->TeleportPlayerToOther(gpBase->mpPlayer, gpBase->mpPlayer2);
		else
			gpBase->mpDebugHandler->SetDebugWindowActive(true);
	}
	if(mpInput->BecameTriggerd(eLuxAction_ReloadMap))
	{
		if(bCoopTeleport)
			gpBase->mpMapHandler->TeleportPlayerToOther(gpBase->mpPlayer2, gpBase->mpPlayer);
		else if(gpBase->mpConfigHandler->mbLoadDebugMenu)
			gpBase->mpDebugHandler->QuickReloadMap();
	}
	if(mpInput->BecameTriggerd(eLuxAction_FastForward) && gpBase->mpConfigHandler->mbLoadDebugMenu)
	{
		bool bActivate = !gpBase->mpDebugHandler->GetFastForward();

		gpBase->mpDebugHandler->SetFastForward(bActivate);
	}

	if(mpPlayer->IsDead()==false && gpBase->mpDebugHandler->GetAllowQuickSave() && gpBase->mbPTestActivated==false)
	{
		if(mpInput->BecameTriggerd(eLuxAction_QuickSave))
		{
			gpBase->mpSaveHandler->AutoSave();
		}
		if(mpInput->BecameTriggerd(eLuxAction_QuickLoad))
		{
			gpBase->mpSaveHandler->AutoLoad(false);
		}
	}

	////////////////////
	//Exit
	if(mpInput->BecameTriggerd(eLuxAction_Exit))
	{
		gpBase->mpEngine->GetUpdater()->SetContainer("MainMenu");
	}

	////////////////////
	//Toggle Crosshair
	if(mpInput->BecameTriggerd(eLuxAction_CrosshairToggle))
	{
		gpBase->mpPlayer->SetShowCrosshair(!gpBase->mpPlayer->GetShowCrosshair());
	}

	////////////////////
	// Player or other
	if(gpBase->mpMessageHandler->IsPauseMessageActive())
	{
		UpdateGameMessageInput();
	}
	else if(gpBase->mpEffectHandler->GetPlayerIsPaused())
	{
		UpdateGameEffectInput();
	}

	//COOP: BOTH players are asked for input every frame, outside that branch.
	//
	//Both pauses above are ONE player's business. cLuxEffectHandler freezes only
	//the player who triggered the effect -- SetPlayerIsPausedFor calls
	//SetActive(false) on them alone -- and mbPlayerIsPaused is just a global bool
	//meaning "somebody is paused". Leaving this call inside the else made the
	//dispatch read that as "everybody is", so one player zooming into a container,
	//or bringing up a pause message, froze the other one solid. Free-roam could not
	//cover for it either: that keys off GetCoopMenuOwner(), which is only non-NULL
	//for the Inventory and Journal containers, and an effect leaves the container
	//on "Default".
	//
	//Safe because each pass early-outs on its own player's IsActive()==false, so
	//whoever triggered the pause stays frozen by the very flag that froze them
	//while the other one carries on.
	//
	//P1 was the half I missed last time: only P2 came out of the branch, so a
	//zoom P2 triggered still skipped the else and left P1 with no input at all.
	UpdateGamePlayerInput();
	UpdatePlayer2Input();
	
}
//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateGamePlayerInput()
{
	mbP1GameInputRan = true;

	//The filter runs at the top of Update() now, for the whole tick and for every
	//state -- see the note there. Calling it a second time here would recompute
	//the edges against state they had already been advanced past, which silently
	//eats every press this function is about to read.

	/////////////////
	// Check if player is dead
	if(mpPlayer->IsDead())
	{
		if(	P1BecameTriggerd(eLuxAction_Attack) ||
			P1BecameTriggerd(eLuxAction_Interact) ||
			P1BecameTriggerd(eLuxAction_Ignite) ||
			P1BecameTriggerd(eLuxAction_Jump) ||
			P1BecameTriggerd(eLuxAction_Crouch) )
		{
			mpPlayer->GetHelperDeath()->OnPressButton();
		}
		return;
	}

	//Coop: the effect and message pauses freeze ONE player, by calling
	//SetActive(false) on them alone. This used to be reached only from the else
	//branch in UpdateGameInput, so the branch did the gating; it is called every
	//frame now and has to gate itself, exactly as UpdatePlayer2Input does.
	//
	//After the death block on purpose, so a dead P1 can still press through the
	//death text. Also covers the level-change fade, which deactivates both
	//players deliberately.
	if(mpPlayer->IsActive()==false) return;
	if(mpPlayer->IsActive()==false) return;

	////////////////////
	// High level
	if(mpPlayer->GetCurrentStateData()->AllowPlayerMenus())
	{
		if(P1BecameTriggerd(eLuxAction_Inventory))
		{
			// In coop mode, the gamepad Back button is handled by UpdatePlayer2Input for P2.
			// Only open P1's inventory from keyboard (Tab).
			bool bOpenP1 = true;
#ifdef USE_GAMEPAD
			if(gpBase->mpMapHandler->GetCoopMode() && IsGamepadPresent() &&
			   !P1KeyIsDown(eKey_Tab))
			{
				bOpenP1 = false; // Gamepad back press — let P2 handle it
			}
#endif
			// Coop: this runs from the Inventory state too now, because P1 keeps
			// playing while P2 is in their bag. There is one cLuxInventory with one
			// active player, so P1 must not be able to seize a bag P2 already has open.
			if(mState != eLuxInputState_Game) bOpenP1 = false;

			if(bOpenP1 && gpBase->mpInventory->GetDisabled()==false)
			{
				gpBase->mpInventory->SetActivePlayer(gpBase->mpPlayer);
				gpBase->mpEngine->GetUpdater()->SetContainer("Inventory");
			}
		}
		// Coop: UpdateGameInput() also runs while P2 is in a menu, so P1 keeps
		// playing. Their keys must not reach INTO that menu -- there is one journal
		// and one inventory, each with a single owner.
		bool bCanOpenMenu = (mState == eLuxInputState_Game);

		//KEYBOARD ONLY. Y is bound to Journal, D-pad right to QuestLog and D-pad left
		//to RecentText, so P2's pad opened P1's journal -- with P1 installed as its
		//owner. Harmless-looking until the gun moved onto Y, at which point it would
		//have fired on every single shot. J, M and N are unaffected.
		if(bCanOpenMenu && SubActionTriggeredForDevice(eLuxAction_Journal, false) &&
			P1BecameTriggerd(eLuxAction_Journal))
		{
			if(gpBase->mpInventory->GetDisabled()==false)
			{
				//Both flags stated, every time. They are latched on the journal and
				//only cleared when it closes, so a journal opened by hand while a
				//stale "read by both" was still set would take the SHARED path in
				//cLuxMapHandler::OnLeaveContainer: world stopped, both halves put
				//away, one full-screen viewport. In dual monitor that viewport
				//covers Player 1's screen only -- so the other player sees nothing
				//at all, and neither of them can move. Cheaper to say it than to
				//prove it can never happen.
				gpBase->mpJournal->SetActivePlayer(gpBase->mpPlayer);
				gpBase->mpJournal->SetShowOnBothPlayers(false);
				gpBase->mpJournal->SetPauseBothPlayers(false);
				gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
			}
		}
		if(bCanOpenMenu && SubActionTriggeredForDevice(eLuxAction_QuestLog, false) &&
			P1BecameTriggerd(eLuxAction_QuestLog))
		{
			if(gpBase->mpInventory->GetDisabled()==false)
			{
				gpBase->mpJournal->SetActivePlayer(gpBase->mpPlayer);
				gpBase->mpJournal->SetShowOnBothPlayers(false);
				gpBase->mpJournal->SetPauseBothPlayers(false);
				gpBase->mpJournal->SetForceInstantExit(true);
				gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
				gpBase->mpJournal->ChangeState(eLuxJournalState_QuestLog);
			}
		}
		if(bCanOpenMenu && SubActionTriggeredForDevice(eLuxAction_RecentText, false) &&
			P1BecameTriggerd(eLuxAction_RecentText))
		{
			if(gpBase->mpInventory->GetDisabled()==false)
			{
				gpBase->mpJournal->SetActivePlayer(gpBase->mpPlayer);
				gpBase->mpJournal->SetShowOnBothPlayers(false);
				gpBase->mpJournal->SetPauseBothPlayers(false);
				gpBase->mpJournal->OpenLastReadText();
			}
		}
	}

	bool bCoopActive = gpBase->mpMapHandler->GetCoopMode();

	/////////////////
	// Possession (debug). Player 1 only -- this whole function acts on mpPlayer,
	// which OnStart binds once to gpBase->mpPlayer.
	//
	// eKey_H read raw rather than through an eLuxAction_*, the same way
	// cLuxPlayer::UpdateNoclip reads eKey_V: the action table is the rebindable
	// PLAYER control set and a debug toggle does not belong in it.
	//
	// H is already bound, to eLuxAction_Holster, which in a debug build is a
	// self-damage cheat -- GiveSanityDamage(10) and GiveDamage(10, ...) further
	// down this function. So drain that edge here, or taking over a monster would
	// cost 10 health every time. Same discard-the-result idiom the co-op
	// direct-key fork uses on Run/Jump/Crouch below.
	cLuxPlayerPossess *pPossess = mpPlayer->GetHelperPossess();
	if(pPossess && ::ImGuiDebugMenu::GetAllowPossession())
	{
		bool bHDown = P1KeyIsDown(eKey_H);
		if(bHDown && mbPossessKeyWasDown==false)
		{
			pPossess->Toggle();
			P1BecameTriggerd(eLuxAction_Holster);
		}
		mbPossessKeyWasDown = bHDown;
	}
	else
	{
		mbPossessKeyWasDown = false;
	}

	/////////////////
	// Enemy morph (debug). Player 1 only, same as possession.
	//
	// 1-4 read raw for the same reason H is, with one difference in our favour:
	// the number keys are bound to no action in gvDefaultInputs at all, so unlike
	// H there is no edge to drain afterwards.
	//
	// eKey_1..eKey_4 are consecutive in eKey (InputTypes.h), which is what lets
	// one loop cover all four.
	{
		const int lMorphKeys = (int)(sizeof(mvMorphKeyWasDown)/sizeof(mvMorphKeyWasDown[0]));
		const bool bCanMorph = pPossess != NULL && ::ImGuiDebugMenu::GetAllowEnemyMorph();

		for(int i=0; i<lMorphKeys && i<(int)eLuxMorphType_LastEnum; ++i)
		{
			//Edges cleared rather than kept while the feature is off, so switching
			//it back on with a key already held does not fire on release.
			bool bDown = bCanMorph && mpInput->GetKeyboard()->KeyIsDown((eKey)(eKey_1 + i));

			if(bDown && mvMorphKeyWasDown[i]==false) pPossess->ToggleMorph(i);

			mvMorphKeyWasDown[i] = bDown;
		}
	}

	// Movement, run, look and the two mouse buttons drive the MONSTER while this
	// is set. Everything else -- inventory, journal, the menus, Escape -- is left
	// alone, so there is always a way out.
	const bool bPossessing = (pPossess != NULL && pPossess->IsPossessing());

	/////////////////
	// Movement Direction
	// When coop is active, use keyboard directly for P1 (gamepad goes to P2)
	if(P1IsTriggerd(eLuxAction_Forward))
	{
#ifdef USE_GAMEPAD
		if(!bCoopActive || !IsGamepadPresent() || P1KeyIsDown(eKey_W))
#endif
		{
			if(bPossessing)	pPossess->AddMove(1, 0);
			else			mpPlayer->Move(eCharDir_Forward, 1);
		}
	}
	if(P1IsTriggerd(eLuxAction_Backward))
	{
#ifdef USE_GAMEPAD
		if(!bCoopActive || !IsGamepadPresent() || P1KeyIsDown(eKey_S))
#endif
		{
			if(bPossessing)	pPossess->AddMove(-1, 0);
			else			mpPlayer->Move(eCharDir_Forward, -1);
		}
	}
	if(P1IsTriggerd(eLuxAction_Right))
	{
#ifdef USE_GAMEPAD
		if(!bCoopActive || !IsGamepadPresent() || P1KeyIsDown(eKey_D))
#endif
		{
			if(bPossessing)	pPossess->AddMove(0, 1);
			else			mpPlayer->Move(eCharDir_Right, 1);
		}
	}
	if(P1IsTriggerd(eLuxAction_Left))
	{
#ifdef USE_GAMEPAD
		if(!bCoopActive || !IsGamepadPresent() || P1KeyIsDown(eKey_A))
#endif
		{
			if(bPossessing)	pPossess->AddMove(0, -1);
			else			mpPlayer->Move(eCharDir_Right, -1);
		}
	}

	/////////////////
	// Lean
	if(P1IsTriggerd(eLuxAction_LeanRight))
	{
		mpPlayer->SetLean(1);
	}
	if(P1IsTriggerd(eLuxAction_LeanLeft))
	{
		mpPlayer->SetLean(-1);
	}

	/////////////////
	// Actions
	// In coop mode, gamepad buttons should only affect P2 (handled in UpdatePlayer2Input)
	// IMPORTANT: Release events (WasTriggerd) must ALWAYS fire for P1 to prevent stuck states.
	// Only press events (BecameTriggerd) need the gamepad guard.

	//Possession takes both mouse buttons. Mouse 1 swings, mouse 2 smashes a door,
	//as asked -- and in this game's default table the LEFT button is
	//eLuxAction_Interact and the RIGHT is eLuxAction_Attack (see the cLuxInput
	//table at the top of this file), so binding them this way round is what puts
	//the PHYSICAL buttons where they were asked for. Through the actions rather
	//than the raw mouse so a rebind still works, with the same pad guard the
	//normal attack uses: in co-op a pad's shoulder buttons belong to P2.
	//
	//The normal action set is skipped entirely while possessing, so Interact
	//cannot also grab whatever P1's parked body happens to be standing next to.
	if(bPossessing)
	{
		if(P1BecameTriggerd(eLuxAction_Interact))
		{
#ifdef USE_GAMEPAD
			if(!bCoopActive || !IsGamepadPresent() || mpInput->GetMouse()->ButtonIsDown(eMouseButton_Left))
#endif
			pPossess->DoAttack();
		}

		if(P1BecameTriggerd(eLuxAction_Attack))
		{
#ifdef USE_GAMEPAD
			if(!bCoopActive || !IsGamepadPresent() || mpInput->GetMouse()->ButtonIsDown(eMouseButton_Right))
#endif
			pPossess->DoBreakDoor();
		}

		//Drain the releases too, or the first frame after letting go of the monster
		//fires a stale WasTriggerd into DoAction.
		P1WasTriggerd(eLuxAction_Interact);
		P1WasTriggerd(eLuxAction_Attack);
		P1BecameTriggerd(eLuxAction_LeftClick);
		P1WasTriggerd(eLuxAction_LeftClick);
	}
	//Attack -- or the gun, if one is up. A raised gun takes the attack button
	//over rather than adding a binding, which is how a weapon is supposed to
	//behave and leaves grab/interact alone.
	else if(mpPlayer->GetHelperGun() && mpPlayer->GetHelperGun()->IsActive())
	{
		if(P1BecameTriggerd(eLuxAction_Attack))
		{
#ifdef USE_GAMEPAD
			if(!bCoopActive || !IsGamepadPresent() || mpInput->GetMouse()->ButtonIsDown(eMouseButton_Right))
#endif
			mpPlayer->GetHelperGun()->Fire();
		}
		P1BecameTriggerd(eLuxAction_Attack);	//eat the edge
	}
	//Attack
	else if(P1BecameTriggerd(eLuxAction_Attack))
	{
#ifdef USE_GAMEPAD
		if(!bCoopActive || !IsGamepadPresent() || mpInput->GetMouse()->ButtonIsDown(eMouseButton_Right))
#endif
		mpPlayer->DoAction(eLuxPlayerAction_Attack, true);
	}
	if(bPossessing==false && P1WasTriggerd(eLuxAction_Attack))
	{
		mpPlayer->DoAction(eLuxPlayerAction_Attack, false);
	}

	//Interact
	if(bPossessing==false && P1BecameTriggerd(eLuxAction_Interact))
	{
#ifdef USE_GAMEPAD
		if(!bCoopActive || !IsGamepadPresent() || mpInput->GetMouse()->ButtonIsDown(eMouseButton_Left))
#endif
		{
			P1BecameTriggerd(eLuxAction_LeftClick); 
			mpPlayer->DoAction(eLuxPlayerAction_Interact ,true);
		}
	}
	if(bPossessing==false && P1WasTriggerd(eLuxAction_Interact))
	{
		P1WasTriggerd(eLuxAction_LeftClick); 
		mpPlayer->DoAction(eLuxPlayerAction_Interact, false);
	}

	//Ignite
	if(P1BecameTriggerd(eLuxAction_Ignite))
	{
#ifdef USE_GAMEPAD
		if(!bCoopActive || !IsGamepadPresent())
#endif
		mpPlayer->DoAction(eLuxPlayerAction_Ignite ,true);
	}
	if(P1WasTriggerd(eLuxAction_Ignite))
	{
		mpPlayer->DoAction(eLuxPlayerAction_Ignite, false);
	}

	//Holster
	if(P1BecameTriggerd(eLuxAction_Holster))	
	{
		if(gpBase->mpConfigHandler->mbLoadDebugMenu)
		{
			mpPlayer->GiveSanityDamage(10);
			mpPlayer->LowerSanity(3, true);//1.0f / 60.0f);
			mpPlayer->GiveDamage(10, 10, eLuxDamageType_BloodSplat, true, true);
		}	
	}
		//mpPlayer->DoAction(eLuxPlayerAction_Holster ,true);
	//if(P1WasTriggerd(eLuxAction_Holster))	mpPlayer->DoAction(eLuxPlayerAction_Holster, false);
	
	//Lantern
	if(P1BecameTriggerd(eLuxAction_Lantern))
	{
#ifdef USE_GAMEPAD
		if(!bCoopActive || !IsGamepadPresent() || P1KeyIsDown(eKey_F))
#endif
		mpPlayer->DoAction(eLuxPlayerAction_Lantern ,true);
	}
	if(P1WasTriggerd(eLuxAction_Lantern))
	{
		mpPlayer->DoAction(eLuxPlayerAction_Lantern, false);
	}

	//Debug gun. Keyboard only -- P2 raises theirs with LB in UpdatePlayer2Input,
	//and G is not bound on the pad at all, so no coop guard is needed here.
	if(::ImGuiDebugMenu::GetDebugGuns() && P1BecameTriggerd(eLuxAction_DebugGun))
	{
		mpPlayer->DoAction(eLuxPlayerAction_DebugGun, true);
	}


	//Scroll
	if(P1BecameTriggerd(eLuxAction_ScrollUp))	mpPlayer->Scroll(1.0f);
	if(P1BecameTriggerd(eLuxAction_ScrollDown))	mpPlayer->Scroll(-1.0f);


	/////////////////
	// Movements (Run / Jump / Crouch)
	//
	// While coop is active with a pad attached, P1 is driven straight off the
	// keyboard with our own edge detection instead of through the shared cAction
	// objects. Reason: eLuxAction_Run/Jump/Crouch each carry BOTH a keyboard and a
	// gamepad sub-action (see gvLuxInputs: eGamepadButton_A->Jump,
	// eGamepadButton_B->Crouch, LeftTrigger->Run). cAction::Update() only clears its
	// "already consumed" latch when NO bound device is down (Action.cpp:202-225), so
	// while P2 holds A, mbIsDown stays true, mbIsTriggerd never resets, and
	// BecameTriggerd() returns false forever -- P1 could not jump at all. The
	// converse leaked the other way: P2 releasing the left trigger produced a
	// WasTriggerd(Run) whose old guard (!KeyIsDown(LeftShift)) passed, calling
	// Run(false) on P1 and killing P1's sprint. Same shape as the coop lean guard
	// below.
#ifdef USE_GAMEPAD
	bool bP1DirectKeys = bCoopActive && IsGamepadPresent();
#else
	bool bP1DirectKeys = false;
#endif

	if(bP1DirectKeys)
	{
		bool bP1Run    = P1KeyIsDown(eKey_LeftShift);
		bool bP1Jump   = P1KeyIsDown(eKey_Space);
		bool bP1Crouch = P1KeyIsDown(eKey_LeftCtrl);

		//Shift runs the MONSTER while possessing. SetRunning is level-triggered, so
		//it wants the held state rather than the edges the player controls use.
		if(bPossessing)
		{
			pPossess->SetRunning(bP1Run);
		}
		else
		{
			if(bP1Run && !mbP1_Run_WasDown)  mpPlayer->Run(true);
			if(!bP1Run && mbP1_Run_WasDown)  mpPlayer->Run(false);
		}
		mbP1_Run_WasDown = bP1Run;

		if(bP1Jump && !mbP1_Jump_WasDown)  mpPlayer->Jump(true);
		if(!bP1Jump && mbP1_Jump_WasDown)  mpPlayer->Jump(false);
		mbP1_Jump_WasDown = bP1Jump;

		if(bP1Crouch && !mbP1_Crouch_WasDown)  mpPlayer->Crouch(true);
		if(!bP1Crouch && mbP1_Crouch_WasDown)  mpPlayer->Crouch(false);
		mbP1_Crouch_WasDown = bP1Crouch;

		//Drain the shared actions so no stale edge fires the instant coop is
		//switched off mid-session. Same discard-the-result idiom as the
		//eLuxAction_LeftClick drains further up.
		P1BecameTriggerd(eLuxAction_Run);
		P1WasTriggerd(eLuxAction_Run);
		P1BecameTriggerd(eLuxAction_Jump);
		P1WasTriggerd(eLuxAction_Jump);
		P1BecameTriggerd(eLuxAction_Crouch);
		P1WasTriggerd(eLuxAction_Crouch);
	}
	else
	{
		//Run
		if(bPossessing)
		{
			pPossess->SetRunning(P1IsTriggerd(eLuxAction_Run));
			P1BecameTriggerd(eLuxAction_Run);
			P1WasTriggerd(eLuxAction_Run);
		}
		else
		{
			if(P1BecameTriggerd(eLuxAction_Run))		mpPlayer->Run(true);
			if(P1WasTriggerd(eLuxAction_Run))		mpPlayer->Run(false);
		}

		//Jump
		if(P1BecameTriggerd(eLuxAction_Jump))	mpPlayer->Jump(true);
		if(P1WasTriggerd(eLuxAction_Jump))		mpPlayer->Jump(false);

		//Crouch
		if(P1BecameTriggerd(eLuxAction_Crouch))	mpPlayer->Crouch(true);
		if(P1WasTriggerd(eLuxAction_Crouch))		mpPlayer->Crouch(false);

		mbP1_Run_WasDown = false;
		mbP1_Jump_WasDown = false;
		mbP1_Crouch_WasDown = false;
	}

	/////////////////
	// Head

	// Mouse
	cVector2l vMouseRelPos = GetFrameMouseRel();

	////////////////////////////////////////////////////////////////////////
	// Take Player 2's mouse back out of it.
	//
	// SDL's relative motion is the SUM of every mouse on the machine -- it has no
	// per-device concept at all -- so without this, Player 2 looking around turns
	// Player 1's head as well.
	//
	// Subtraction rather than suppression, because raw input is an EXTRA stream
	// running beside SDL, not a replacement: SDL's event is already on its way and
	// SDL2's message hook returns void, so there is nothing to swallow. What there
	// is, is arithmetic -- the total is known, Player 2's share is known exactly
	// from WM_INPUT, and the difference is Player 1's. No reliance on message
	// ordering, and it stays right however many mice are plugged in.
	{
		cRawInputWin32 *pRaw = cRawInputWin32::GetInstance();

		if(::ImGuiDebugMenu::GetP2UsesRawInput() && pRaw && pRaw->IsAvailable())
		{
			//GetP2RawMouse, not GetP2RawDevice. A keyboard and a mouse are two
			//different raw devices with two different handles, so asking Player 2's
			//KEYBOARD how far it moved answers zero however hard they swing the
			//mouse -- nothing was subtracted, and Player 2 aiming turned Player 1's
			//head with them.
			int lP2X = 0, lP2Y = 0;
			pRaw->GetRelMotionForDevice(::ImGuiDebugMenu::GetP2RawMouse(), &lP2X, &lP2Y);

			//////////////////////////////////////////////////////////////////
			// A SUBTRACTION NEEDS SOMETHING TO SUBTRACT FROM.
			//
			// cMouseSDL::Update throws the polled relative motion away while the
			// console or the debug menu is up, so the total arriving here is zero --
			// and zero minus Player 2's share is MINUS Player 2's share. Player 1's
			// head turned backwards to Player 2's mouse for as long as the console
			// was open, which is a worse version of the bug that subtraction is
			// there to fix.
			//
			// The overlay is modal for Player 1: no total, no look, nothing to take
			// a share out of. Player 2 is still out in the world and their own raw
			// path keeps driving them -- it never went through SDL to begin with.
			if(cImGuiConsole::IsVisible() || ::ImGuiDebugMenu::IsVisible())
			{
				vMouseRelPos = cVector2l(0,0);
			}
			else
			{
				vMouseRelPos.x -= lP2X;
				vMouseRelPos.y -= lP2Y;
			}
		}
		else if(cImGuiConsole::IsVisible() || ::ImGuiDebugMenu::IsVisible())
		{
			//Single player, or Player 2 on a pad: the overlay is still modal.
			vMouseRelPos = cVector2l(0,0);
		}
	}

	cVector2f vMouseRelPosFloat = cVector2f((float)vMouseRelPos.x, (float)vMouseRelPos.y)*mfMouseSensitivity;
	cVector2l vAbsRel = cMath::RoundToInt(vMouseRelPosFloat);
	cVector2f vRelPos = cVector2f((float)vAbsRel.x,(float)vAbsRel.y) / (1.7f * mpGraphics->GetLowLevel()->GetScreenSizeFloat().y);
	cVector2f vFinalPos;

	//Check if position should be smoothed.
	if(mbSmoothMouse)	vFinalPos = GetSmoothMousePos(vRelPos);
	else				vFinalPos = vRelPos;

	//Invert the Y-axis
	if(mbInvertMouse)
	{
		vFinalPos.y = -vFinalPos.y;
	}

	if(bCoopActive ? P1KeyIsDown(eKey_LeftAlt) : P1IsTriggerd(eLuxAction_Lean))
	{
		mpPlayer->AddLean(vRelPos.x);
	}

#ifdef USE_GAMEPAD
	//////////////////////////////////////////
	// Gamepad movement and look
	// When coop is active, gamepad input goes to Player 2 only
	if(IsGamepadPresent() && !bCoopActive)
	{
#if USE_SDL2
		if(mpPad->ButtonIsDown(eGamepadButton_DpadUp) || mpPad->ButtonIsDown(eGamepadButton_DpadDown))
#else
		if(mpPad->HatIsInState(eGamepadHat_0, eGamepadHatState_Up) || mpPad->HatIsInState(eGamepadHat_0, eGamepadHatState_Down))
#endif
		{
			if(P1IsTriggerd(eLuxAction_ZoomOut))	mpPlayer->Scroll( gpBase->mpEngine->GetFrameTime() * 8.0f);
			if(P1IsTriggerd(eLuxAction_ZoomIn))	mpPlayer->Scroll(-gpBase->mpEngine->GetFrameTime() * 8.0f);
		}

		//////////////////////////////////////////
		// Walk
		//cVector2f vAnalogWalkAxis = cVector2f(mpPad->GetAxisValue(eGamepadAxis_0), mpPad->GetAxisValue(eGamepadAxis_1));
		
		//if(cMath::Abs(vAnalogWalkAxis.x) > 0.05f)
		//	mpPlayer->Move(eCharDir_Right, vAnalogWalkAxis.x);
		//if(cMath::Abs(vAnalogWalkAxis.y) > 0.05f)
		//	mpPlayer->Move(eCharDir_Forward, -vAnalogWalkAxis.y);
		

		//////////////////////////////////////////
		// Look / Lean
#if USE_SDL2
		cVector2f vAnalogLookAxis = cVector2f(mpPad->GetAxisValue(eGamepadAxis_RightX), mpPad->GetAxisValue(eGamepadAxis_RightY));
#else
		cVector2f vAnalogLookAxis = cVector2f(mpPad->GetAxisValue(eGamepadAxis_4), mpPad->GetAxisValue(eGamepadAxis_3));
#endif

		
		{
			if(P1IsTriggerd(eLuxAction_Rotate))
			{
				mpPlayer->Scroll(-gpBase->mpEngine->GetFrameTime() * 6.0f * vAnalogLookAxis.y);

#if USE_SDL2
				vAnalogLookAxis = cVector2f(mpPad->GetAxisValue(eGamepadAxis_LeftX), mpPad->GetAxisValue(eGamepadAxis_LeftY));
#else
				vAnalogLookAxis = cVector2f(mpPad->GetAxisValue(eGamepadAxis_0), mpPad->GetAxisValue(eGamepadAxis_1));
#endif
				cVector2f vE = cMath::Vector2Abs(vAnalogLookAxis);	vE.x = sqrtf(vE.x); vE.y = sqrtf(vE.y);
				vAnalogLookAxis = vE * vAnalogLookAxis * mfGamepadLookSensitivity / 1.25f;

				if(vAnalogLookAxis.Length() > 0)
				{
					mpPlayer->GetCharacterBody()->StopMovement();
				}
			}

			///////////////
			// Make up for the dead zone
			vAnalogLookAxis -= cVector2f(cMath::Sign(vAnalogLookAxis.x), cMath::Sign(vAnalogLookAxis.y)) * mpPad->GetAxisDeadZoneRadiusValue();
			vAnalogLookAxis *= 1.0f / (1.0f - mpPad->GetAxisDeadZoneRadiusValue());

			cVector2f vExponent = cMath::Vector2Abs(vAnalogLookAxis);	vExponent.x = sqrtf(vExponent.x); vExponent.y = sqrtf(vExponent.y);
			cVector2f vGamepadPos = (vAnalogLookAxis * vExponent) * mfGamepadLookSensitivity*gpBase->mpEngine->GetStepSize();

			//Invert the Y-axis
			if(mbGamepadLookInvert)
			{
				vGamepadPos.y = -vGamepadPos.y;
			}

			vFinalPos += vGamepadPos;
		}		

		if(P1IsTriggerd(eLuxAction_Lean))
		{
			if(cMath::Abs(vAnalogLookAxis.x) > 0.05f) mpPlayer->SetLean(vAnalogLookAxis.x);
			else									  mpPlayer->SetLean(0);
		}
	}
#endif
	if(bCoopActive ? P1KeyIsDown(eKey_LeftAlt) : P1IsTriggerd(eLuxAction_Lean))
	{
		vFinalPos = 0;
	}

	//Possessing: the mouse orbits the chase camera instead of turning P1. The
	//monster itself is turned by the mover, from the goal point the movement keys
	//build relative to wherever this camera ends up pointing.
	if(bPossessing)
	{
		pPossess->AddLook(-vFinalPos.x, -vFinalPos.y);
	}
	else
	{
		mpPlayer->AddYaw(-vFinalPos.x);
		mpPlayer->AddPitch(-vFinalPos.y);
	}

	/////////////////
	// Hand the possessed monster this tick's steering.
	//
	// Last thing in the function on purpose: both the movement keys and the mouse
	// have now been read, so the goal it gets is built from this tick's input and
	// this tick's camera angles.
	//
	// And from HERE rather than from the helper's own Update because of where the
	// two sit in the tick. This handler is a GLOBAL module, so it runs before the
	// "Default" container -- and cLuxMapHandler, which updates the monster, is the
	// first module in it. Pushing here means the monster turns and brakes on a
	// heading built this tick; pushing from cLuxPlayer, two modules further down,
	// meant it always used the previous one.
	if(bPossessing) pPossess->PushSteerToEnemy();
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::SubActionTriggeredForDevice(int alAction, bool abGamepad)
{
	cAction *pAction = mpInput->GetAction(alAction);
	if(pAction == NULL) return false;

	//Player 2's device out of the answer, when they own one. Null otherwise, and
	//then every branch below falls through to plain IsTriggerd as it always did.
	cRawInputWin32 *pRaw = mbP1ActionFilterActive ? cRawInputWin32::GetInstance() : NULL;

	//GetInputType() is "Keyboard", "MouseButton", "GamepadButton", "GamepadAxis",
	//"GamepadHat" -- the pad ones all share a prefix.
	for(size_t i=0; i<pAction->GetSubActionNum(); ++i)
	{
		iSubAction *pSubAction = pAction->GetSubAction(i);
		if(pSubAction == NULL) continue;

		const tString sType = pSubAction->GetInputType();

		bool bIsPadInput = sType.find("Gamepad") != tString::npos;
		if(bIsPadInput != abGamepad) continue;

		//////////////////////////////////////////////////////////////////
		// "THE KEYBOARD" MEANS PLAYER 1'S KEYBOARD.
		//
		// iSubAction::IsTriggerd() goes to SDL, which has one keyboard and one
		// mouse for the whole machine. Every caller here is asking "was this done
		// on the keyboard, as opposed to the pad", and in this mode that question
		// has two possible answers while SDL only knows one of them.
		//
		// What that cost: Player 2 pressing J passed this test as Player 1, so
		// PLAYER 1'S journal opened. It drew on Player 1's half, where Player 2
		// could not see it -- "they still do not see any journal" -- and because
		// the journal then belonged to Player 1, free roam handed Player 1's input
		// to Player 2 and Player 1 could not look around. One wrong answer, all
		// three symptoms.
		//
		// The pad branch needs none of this: there is one pad and it is Player 2's.
		if(pRaw && abGamepad==false)
		{
			if(sType == "Keyboard")
			{
				cActionKeyboard *pKey = static_cast<cActionKeyboard*>(pSubAction);
				if(pRaw->KeyIsDownExcludingDevice(::ImGuiDebugMenu::GetP2RawDevice(), pKey->GetKey()))
					return true;
				continue;
			}

			if(sType == "MouseButton")
			{
				cActionMouseButton *pButton = static_cast<cActionMouseButton*>(pSubAction);

				//Left 0, middle 1, right 2 -- the three raw input reports as
				//buttons. The wheel is not one and is left to SDL.
				const int lButton = (int)pButton->GetButton();

				if(lButton <= 2 && P2RawMouseKnown())
				{
					if(pRaw->ButtonIsDownExcludingDevice(::ImGuiDebugMenu::GetP2RawMouse(), lButton))
						return true;
					continue;
				}

				if(pSubAction->IsTriggerd()) return true;
				continue;
			}
		}

		if(pSubAction->IsTriggerd()) return true;
	}

	return false;
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::MenuActionTriggered(int alAction)
{
	bool bPadOwns = CoopMenuAcceptsGamepad();
	bool bKeyOwns = CoopMenuAcceptsKeyboard();

	//Nothing open, single player, or a world note both players are reading:
	//either device may act, and this is plain BecameTriggerd.
	if(bPadOwns && bKeyOwns) return mpInput->BecameTriggerd(alAction);

	//Ask the OWNER's device first, without consuming. Reading BecameTriggerd here
	//to then reject it would clear the edge and swallow the other player's press
	//before their gameplay input ever runs -- that is what stopped P1 grabbing
	//anything while P2 had a bag open.
	if(SubActionTriggeredForDevice(alAction, bPadOwns)==false) return false;

	return mpInput->BecameTriggerd(alAction);
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::MenuActionHeld(int alAction)
{
	bool bPadOwns = CoopMenuAcceptsGamepad();
	bool bKeyOwns = CoopMenuAcceptsKeyboard();

	if(bPadOwns && bKeyOwns) return mpInput->IsTriggerd(alAction);

	//SubActionTriggeredForDevice is already a level read, so this is the whole
	//answer -- and unlike the edge version there is nothing to consume afterwards.
	return SubActionTriggeredForDevice(alAction, bPadOwns);
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::CoopMenuIsShared()
{
	if(gpBase->mpMapHandler->GetCoopMode()==false) return false;

	//Only the journal ever goes to both. A bag belongs to one player by
	//definition -- it holds their things.
	if(gpBase->mpEngine->GetUpdater()->GetCurrentContainerName() != "Journal") return false;

	return gpBase->mpJournal->GetShowOnBothPlayers();
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::CoopMenuAcceptsKeyboard()
{
	//Both of them are reading it and the world is stopped. Whoever reaches for a
	//key first turns the page for the pair; that is the whole point.
	if(CoopMenuIsShared()) return true;

	cLuxPlayer *pOwner = GetCoopMenuOwner();
	if(pOwner == NULL) return true;

	return pOwner->IsPlayer2()==false;
}

bool cLuxInputHandler::CoopMenuAcceptsGamepad()
{
	if(CoopMenuIsShared()) return true;

	cLuxPlayer *pOwner = GetCoopMenuOwner();
	if(pOwner == NULL) return true;

	return pOwner->IsPlayer2();
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::CoopMenuAcceptsP2Raw()
{
	if(::ImGuiDebugMenu::GetP2UsesRawInput()==false) return false;
	if(gpBase->mpMapHandler->GetCoopMode()==false) return false;

	cRawInputWin32 *pRaw = cRawInputWin32::GetInstance();
	if(pRaw==NULL || pRaw->IsAvailable()==false) return false;

	if(CoopMenuIsShared()) return true;

	cLuxPlayer *pOwner = GetCoopMenuOwner();
	return pOwner != NULL && pOwner->IsPlayer2();
}

//-----------------------------------------------------------------------

cViewport* cLuxInputHandler::GetCoopMenuViewport()
{
	if(mState == eLuxInputState_Inventory)	return gpBase->mpInventory->GetViewport();
	if(mState == eLuxInputState_Journal)	return gpBase->mpJournal->GetViewport();

	return NULL;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// ALWAYS Player 1's devices, a shared note included.
//
// A note they are both reading used to take the UNFILTERED action here, on the
// grounds that either of them may turn the page. True, but this path clicks at
// PLAYER 1'S pointer -- so Player 2's left button pressed whatever Player 1
// happened to be hovering over, from the other side of the room.
//
// Player 2's clicks now go in through SendP2RawInputToGui at Player 2's own
// pointer instead, which is the whole point of there being two of them.
//////////////////////////////////////////////////////////////////////////
bool cLuxInputHandler::GuiActionBecame(int alAction)
{
	return P1BecameTriggerd(alAction);
}

bool cLuxInputHandler::GuiActionWas(int alAction)
{
	return P1WasTriggerd(alAction);
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::P2RawMouseKnown()
{
	if(::ImGuiDebugMenu::GetP2UsesRawInput()==false) return false;

	cRawInputWin32 *pRaw = cRawInputWin32::GetInstance();
	if(pRaw==NULL || pRaw->IsAvailable()==false) return false;

	//////////////////////////////////////////////////////////////////////
	// RAW MOUSE DATA ONLY EXISTS WHILE RELATIVE MOUSE MODE IS ON.
	//
	// This code registers the raw KEYBOARD itself and leans on SDL for the raw
	// mouse, because SDL registers usage 1/2 for its own relative mode -- and
	// SDL REMOVES that registration the moment relative mode goes off. The
	// menus do exactly that: cLuxMainMenu::OnEnterContainer and
	// cLuxPreMenu::OnEnterContainer both call RelativeMouse(false).
	//
	// With the registration gone, no WM_INPUT mouse messages arrive at all and
	// every per-device button state freezes at whatever it last held. It does
	// not report an error, it reports "nobody is pressing anything" -- forever.
	// So Player 1's clicks were filtered against a snapshot instead of against
	// the live mouse, and the pause menu could not be clicked.
	//
	// Saying so here rather than at the call sites: everything that filters a
	// mouse button already asks this first, and the honest answer while the
	// stream is down is that Player 2's mouse cannot be told apart at all. The
	// callers then fall back to SDL, which is both players' mice together --
	// correct for a full-screen menu, where there is only one cursor anyway.
#if USE_SDL2
	if(SDL_GetRelativeMouseMode() == SDL_FALSE) return false;
#endif

	const std::map<hpl::tRawInputDevice, hpl::cRawInputDeviceState> &mapDev = pRaw->GetDevices();
	std::map<hpl::tRawInputDevice, hpl::cRawInputDeviceState>::const_iterator it =
		mapDev.find((hpl::tRawInputDevice)::ImGuiDebugMenu::GetP2RawMouse());

	return it != mapDev.end() && it->second.mbIsMouse;
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateP2MousePairing()
{
	if(::ImGuiDebugMenu::GetP2UsesRawInput()==false) return;
	if(::ImGuiDebugMenu::GetP2RawMouseUserSet()) return;
	if(gpBase->mpMapHandler->GetCoopMode()==false) return;

	cRawInputWin32 *pRaw = cRawInputWin32::GetInstance();
	if(pRaw==NULL || pRaw->IsAvailable()==false) return;

	void *pKeyboard = ::ImGuiDebugMenu::GetP2RawDevice();

	//////////////////////////////////////////////////////////////////////
	// A streamed guest needs no pairing at all: injected input carries no device
	// behind it, keyboard and mouse alike, so both are the same zero handle.
	if(pKeyboard == kRawInputInjectedDevice)
	{
		::ImGuiDebugMenu::SetP2RawMouseAuto(kRawInputInjectedDevice);
		return;
	}

	//Already pointing at something that really is a mouse and really is sending:
	//nothing to work out.
	{
		const std::map<hpl::tRawInputDevice, hpl::cRawInputDeviceState> &mapDone = pRaw->GetDevices();
		std::map<hpl::tRawInputDevice, hpl::cRawInputDeviceState>::const_iterator doneIt =
			mapDone.find((hpl::tRawInputDevice)::ImGuiDebugMenu::GetP2RawMouse());

		if(doneIt != mapDone.end() && doneIt->second.mbIsMouse) return;
	}

	//////////////////////////////////////////////////////////////////////
	// Is Player 2 driving right now?
	//
	// Their own keyboard, their movement keys. Player 1 cannot press these --
	// they are on a different keyboard -- so anything that moves while one of
	// them is held is being moved by Player 2's hand.
	bool bP2Moving = false;
	for(int i=0; i<(int)eKey_LastEnum && bP2Moving==false; ++i)
	{
		if(pRaw->KeyIsDownOnDevice(pKeyboard, (eKey)i)) bP2Moving = true;
	}

	if(bP2Moving==false) return;

	//////////////////////////////////////////////////////////////////////
	// Score every mouse that moved this frame, then take the winner -- but only
	// once it is properly ahead.
	//
	// The margin is what makes this safe. Player 1 moving their own mouse during
	// the same second scores too, so a bare maximum could be decided by one noisy
	// frame. Requiring twice the runner-up means the mouse actually being used
	// alongside Player 2's keys has to win it, not merely lead it.
	const std::map<hpl::tRawInputDevice, hpl::cRawInputDeviceState> &mapDev = pRaw->GetDevices();
	std::map<hpl::tRawInputDevice, hpl::cRawInputDeviceState>::const_iterator devIt = mapDev.begin();

	for(; devIt != mapDev.end(); ++devIt)
	{
		if(devIt->second.mbIsMouse==false) continue;
		if(devIt->second.mlRelX == 0 && devIt->second.mlRelY == 0) continue;

		mmapP2MouseScore[(void*)devIt->first] += 1;
	}

	void *pBest = NULL;
	int lBest = 0, lSecond = 0;

	std::map<void*, int>::const_iterator scoreIt = mmapP2MouseScore.begin();
	for(; scoreIt != mmapP2MouseScore.end(); ++scoreIt)
	{
		if(scoreIt->second > lBest)
		{
			lSecond = lBest;
			lBest = scoreIt->second;
			pBest = scoreIt->first;
		}
		else if(scoreIt->second > lSecond)
		{
			lSecond = scoreIt->second;
		}
	}

	//About an eighth of a second of Player 2 holding a key while moving a mouse,
	//and a clear winner -- both, not either. Deliberately low: every frame spent
	//unpaired is a frame Player 2's aim drags Player 1's view with it.
	if(pBest != NULL && lBest >= 8 && lBest >= lSecond + 4)
	{
		::ImGuiDebugMenu::SetP2RawMouseAuto(pBest);
		Log("Player 2 mouse paired automatically: %p (score %d, next %d)\n", pBest, lBest, lSecond);
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::ResetPlayer2RawGuiState()
{
	mbP2Raw_GuiLeft_WasDown = false;
	mbP2Raw_GuiRight_WasDown = false;
	mbP2Raw_GuiExit_WasDown = false;
	mbP2Raw_GuiInventory_WasDown = false;
	mbP2Raw_GuiJournal_WasDown = false;
	mbP2Raw_NoteAdvance_WasDown = false;

	//The pointer belongs to whatever menu is open. Dropping it here is what makes
	//the next menu seed its cursor in the middle of ITS OWN half instead of
	//wherever the last one left it.
	mbCoopCursorValid = false;

	if(::ImGuiDebugMenu::GetP2UsesRawInput()==false) return;

	cRawInputWin32 *pRaw = cRawInputWin32::GetInstance();
	if(pRaw==NULL || pRaw->IsAvailable()==false) return;

	//Seeded from the LIVE device, not to false. The key that opened the menu is
	//still held on the frame the menu comes up, and seeding to false would read
	//that as a fresh press and close it again immediately -- the same reason the
	//pad's Back button is seeded this way.
	void *pDevice = ::ImGuiDebugMenu::GetP2RawDevice();
	void *pMouse  = ::ImGuiDebugMenu::GetP2RawMouse();

	mbP2Raw_GuiLeft_WasDown  = pRaw->ButtonIsDownOnDevice(pMouse, 0);
	mbP2Raw_GuiRight_WasDown = pRaw->ButtonIsDownOnDevice(pMouse, 2);
	mbP2Raw_GuiExit_WasDown  = pRaw->KeyIsDownOnDevice(pDevice, eKey_Escape);
	mbP2Raw_GuiInventory_WasDown = pRaw->KeyIsDownOnDevice(pDevice, eKey_Tab) ||
									mbP2Raw_GuiExit_WasDown;
	mbP2Raw_GuiJournal_WasDown   = pRaw->KeyIsDownOnDevice(pDevice, eKey_J) ||
									pRaw->KeyIsDownOnDevice(pDevice, eKey_N) ||
									pRaw->KeyIsDownOnDevice(pDevice, eKey_M) ||
									pRaw->KeyIsDownOnDevice(pDevice, eKey_Tab) ||
									mbP2Raw_GuiExit_WasDown;

	//////////////////////////////////////////////////////////////////////
	// AND THE PAGE TURN. This one was left at false, and it is why nobody ever
	// saw a note Player 2 picked up.
	//
	// A note is picked up by LEFT CLICKING it. The journal opens on that click,
	// ChangeState lands here, and the button is still physically held -- so a
	// tracker seeded to false reads the very next frame as a fresh press. On a
	// shared note that press is "turn the page", and on a note with one page
	// turning the page closes it. Opened and shut inside two frames, which is
	// exactly the open-then-close sound with nothing in between.
	//
	// It could only ever happen to a note in the WORLD, because the page-turn
	// handler is gated on the note being shared -- so N worked, the pickup did
	// not, and the two looked like unrelated bugs.
	mbP2Raw_NoteAdvance_WasDown = pRaw->ButtonIsDownOnDevice(pMouse, 0) ||
									pRaw->KeyIsDownOnDevice(pDevice, eKey_Space);
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateCoopMenuCursor()
{
	mvCoopCursorRel = 0;

	cRawInputWin32 *pRaw = cRawInputWin32::GetInstance();

	//////////////////////////////////////////////////////////////////////
	// ONLY for the two menus that belong to ONE player.
	//
	// A full-screen menu -- pause, options, credits, the pre-menu -- has no
	// second player to separate from, and cLuxMainMenu::OnEnterContainer turns
	// RELATIVE MOUSE OFF for it. The pointer there is meant to be the operating
	// system's cursor, read absolutely. Feeding it an integrated delta instead
	// makes it drift away from the arrow the player is actually looking at, and
	// clicks land wherever the integration happens to have wandered to. That is
	// the pause menu being hard to click, and it arrived with this function.
	const bool bPerPlayerMenu = (mState == eLuxInputState_Inventory ||
								mState == eLuxInputState_Journal);

	const bool bSeparate =	::ImGuiDebugMenu::GetP2UsesRawInput() &&
							pRaw != NULL && pRaw->IsAvailable() &&
							gpBase->mpMapHandler->GetCoopMode() &&
							bPerPlayerMenu &&
							CurrentStateSendsInputToGui();

	if(bSeparate==false)
	{
		mbCoopCursorValid = false;
		mbCoopCursorP2Valid = false;
		return;
	}

	//////////////////////////////////////////////////////////////////////
	// ONE CURSOR, TWO MICE.
	//
	// SDL's relative motion is the sum of every mouse on the machine, so the
	// free player aiming around the room drags their partner's pointer off
	// whatever they were about to click. That is why nobody could hit the arrow
	// on a note: the pointer was never where it was left.
	//
	// So the pointer is driven from ONE device, decided by who owns the menu:
	//
	//   shared note   -> everything SDL saw. Both of them are reading it and
	//                    either may turn the page, so both mice move it.
	//   P2's menu     -> P2's device alone.
	//   anything else -> SDL minus P2's share, which is P1's mouse exactly.
	const bool bShared = CoopMenuIsShared();
	const bool bP2Owns = bShared==false && CoopMenuAcceptsP2Raw();

	int lP2X = 0, lP2Y = 0;
	pRaw->GetRelMotionForDevice(::ImGuiDebugMenu::GetP2RawMouse(), &lP2X, &lP2Y);

	const cVector2l vAll = GetFrameMouseRel();

	//Player 1's share of the total, which is what is left after Player 2's is
	//taken out of it.
	const cVector2l vP1Delta(vAll.x - lP2X, vAll.y - lP2Y);

	cVector2l vDelta;
	if(bP2Owns)
	{
		vDelta = cVector2l(lP2X, lP2Y);
	}
	else if(bShared)
	{
		//////////////////////////////////////////////////////////////////
		// TWO POINTERS, ONE EACH.
		//
		// A note they are both reading is one cGuiSet, and a cGuiSet has one
		// cursor -- so THIS one stays Player 1's, fed by Player 1's mouse alone,
		// and Player 2 gets a second position of their own at the end of this
		// function.
		//
		// It used to be a single pointer that changed hands to whoever moved
		// last. That is exactly what "Player 2 moving their cursor also moves
		// mine" was: there was only ever one of them, so reaching for your mouse
		// took it off your partner mid-click.
		vDelta = vP1Delta;
	}
	else
	{
		vDelta = vP1Delta;
	}

	//////////////////////////////////////////////////////////////////////
	// Where it is allowed to be.
	//
	// A per-player menu is drawn inside that player's viewport, so a pointer that
	// wandered outside it would be drawn off the edge of their half -- on the
	// other player's monitor, in dual-monitor. A shared note is full screen and
	// gets the whole thing.
	const cVector2f vScreen = mpGraphics->GetLowLevel()->GetScreenSizeFloat();

	cVector2l vMin(0,0);
	cVector2l vMax((int)vScreen.x - 1, (int)vScreen.y - 1);

	cViewport *pVp = bShared ? NULL : GetCoopMenuViewport();
	if(pVp && pVp->GetSize().x > 0 && pVp->GetSize().y > 0)
	{
		vMin = pVp->GetPosition();
		vMax = cVector2l(	vMin.x + pVp->GetSize().x - 1,
							vMin.y + pVp->GetSize().y - 1);
	}

	if(mbCoopCursorValid==false)
	{
		mvCoopCursorPos = cVector2l((vMin.x + vMax.x)/2, (vMin.y + vMax.y)/2);
		mbCoopCursorValid = true;
		mvCoopCursorRel = 0;

		//Seeded on the same frame as Player 1's, or the note's first frame would
		//leave Player 2 without a pointer at all.
		UpdateCoopSharedCursorP2(bShared, lP2X, lP2Y, vMin, vMax);
		return;
	}

	const cVector2l vBefore = mvCoopCursorPos;

	mvCoopCursorPos = mvCoopCursorPos + vDelta;

	if(mvCoopCursorPos.x < vMin.x) mvCoopCursorPos.x = vMin.x;
	if(mvCoopCursorPos.y < vMin.y) mvCoopCursorPos.y = vMin.y;
	if(mvCoopCursorPos.x > vMax.x) mvCoopCursorPos.x = vMax.x;
	if(mvCoopCursorPos.y > vMax.y) mvCoopCursorPos.y = vMax.y;

	//The MOVE that actually happened, after clamping -- a pointer pinned against
	//an edge must report no motion, or the GUI keeps being told it is moving.
	mvCoopCursorRel = mvCoopCursorPos - vBefore;

	UpdateCoopSharedCursorP2(bShared, lP2X, lP2Y, vMin, vMax);
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateCoopSharedCursorP2(bool abShared, int alP2X, int alP2Y,
											   const cVector2l &avMin, const cVector2l &avMax)
{
	//////////////////////////////////////////////////////////////////////
	// PLAYER 2'S POINTER, alongside Player 1's rather than instead of it.
	//
	// Only for a shared note. Every other menu belongs to ONE player and the
	// pointer above is already theirs -- a second one there would be an extra
	// cursor on a screen whose owner is not allowed to touch the menu at all.
	//
	// Same screen-pixel space and the same bounds as Player 1's: a shared note
	// is drawn full screen and mirrored onto the second monitor, so both
	// pointers live in the one virtual page and each player can see where their
	// partner is pointing.
	if(abShared==false)
	{
		mbCoopCursorP2Valid = false;
		return;
	}

	if(mbCoopCursorP2Valid==false)
	{
		//Seeded off to one side of Player 1's, so two pointers that begin life in
		//the same pixel are not mistaken for one that will not move.
		mvCoopCursorPosP2 = cVector2l((avMin.x + avMax.x)/2 + (avMax.x - avMin.x)/8,
									  (avMin.y + avMax.y)/2);
		mbCoopCursorP2Valid = true;
	}
	else
	{
		mvCoopCursorPosP2 = mvCoopCursorPosP2 + cVector2l(alP2X, alP2Y);
	}

	if(mvCoopCursorPosP2.x < avMin.x) mvCoopCursorPosP2.x = avMin.x;
	if(mvCoopCursorPosP2.y < avMin.y) mvCoopCursorPosP2.y = avMin.y;
	if(mvCoopCursorPosP2.x > avMax.x) mvCoopCursorPosP2.x = avMax.x;
	if(mvCoopCursorPosP2.y > avMax.y) mvCoopCursorPosP2.y = avMax.y;
}

//-----------------------------------------------------------------------

void cLuxInputHandler::SendP2RawInputToGui()
{
	cRawInputWin32 *pRaw = cRawInputWin32::GetInstance();
	if(pRaw==NULL || pRaw->IsAvailable()==false) return;

	void *pMouse = ::ImGuiDebugMenu::GetP2RawMouse();
	cGui *pGui = gpBase->mpEngine->GetGui();

	//Left and right only. The wheel is not a button to raw input, and nothing in
	//a bag or a note needs the middle one.
	const bool bLeft = pRaw->ButtonIsDownOnDevice(pMouse, 0);

	if(bLeft && mbP2Raw_GuiLeft_WasDown==false)
	{
		pGui->SendMouseClickDown(eGuiMouseButton_Left);

		//////////////////////////////////////////////////////////////////
		// THE DOUBLE CLICK, which raw input does not have and the inventory
		// cannot do without.
		//
		// cLuxInventory_Slot registers eGuiMessage_MouseDoubleClick and that is
		// how an item is USED -- down and up alone only pick it up and put it
		// back. Player 1 gets it from cInput::DoubleTriggerd, which counts SDL's
		// clicks; Player 2's buttons never go near SDL's action system, so
		// nothing was counting theirs. They could hover, they could drag, and
		// they could not use a single thing in their own bag.
		//
		// Same 0.3s window Player 1's path uses, so both players' bags behave
		// identically rather than one of them needing a different rhythm.
		const double fNow = gpBase->mpEngine->GetGameTime();

		if(mfP2LastGuiClickTime > 0.0 && (fNow - mfP2LastGuiClickTime) <= 0.3)
		{
			pGui->SendMouseDoubleClick(eGuiMouseButton_Left);
			mfP2LastGuiClickTime = -1.0;	//a third click starts a new pair
		}
		else
		{
			mfP2LastGuiClickTime = fNow;
		}
	}

	if(bLeft==false && mbP2Raw_GuiLeft_WasDown)	pGui->SendMouseClickUp(eGuiMouseButton_Left);
	mbP2Raw_GuiLeft_WasDown = bLeft;

	const bool bRight = pRaw->ButtonIsDownOnDevice(pMouse, 2);
	if(bRight && mbP2Raw_GuiRight_WasDown==false)	pGui->SendMouseClickDown(eGuiMouseButton_Right);
	if(bRight==false && mbP2Raw_GuiRight_WasDown)	pGui->SendMouseClickUp(eGuiMouseButton_Right);
	mbP2Raw_GuiRight_WasDown = bRight;
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::P2RawGuiHasButtonEdge()
{
	cRawInputWin32 *pRaw = cRawInputWin32::GetInstance();
	if(pRaw==NULL || pRaw->IsAvailable()==false) return false;

	void *pMouse = ::ImGuiDebugMenu::GetP2RawMouse();

	if(pRaw->ButtonIsDownOnDevice(pMouse, 0) != mbP2Raw_GuiLeft_WasDown)  return true;
	if(pRaw->ButtonIsDownOnDevice(pMouse, 2) != mbP2Raw_GuiRight_WasDown) return true;

	return false;
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::GetCoopSharedCursorP2Virtual(cGuiSet *apSet, cVector2f &avOut)
{
	if(apSet==NULL || mbCoopCursorP2Valid==false) return false;

	//The same arithmetic cGui::SendMousePos does, so the pointer is DRAWN where
	//a click at that position would land. Two copies of one conversion is not
	//ideal; two copies that disagree is a cursor you cannot hit anything with.
	const cVector2f vScreen = mpGraphics->GetLowLevel()->GetScreenSizeFloat();
	if(vScreen.x <= 0 || vScreen.y <= 0) return false;

	cVector2f vVirtual = cVector2f((float)mvCoopCursorPosP2.x, (float)mvCoopCursorPosP2.y) / vScreen;

	vVirtual *= apSet->GetVirtualSize();
	vVirtual -= apSet->GetVirtualSizeOffset();

	avOut = vVirtual;
	return true;
}

//-----------------------------------------------------------------------

cLuxPlayer* cLuxInputHandler::GetCoopMenuOwner()
{
	if(gpBase->mpMapHandler->GetCoopMode()==false) return NULL;

	//Keyed on the CONTAINER, not mState. The menus park the input state at
	//eLuxInputState_Null while they fade, and the journal briefly hands it back to
	//_Game for narrated diaries -- so anything driven off mState quietly stopped
	//believing a menu was open, which is what froze P2 while notes were up. The
	//container is the honest answer: it does not change until the menu is gone.
	tString sContainer = gpBase->mpEngine->GetUpdater()->GetCurrentContainerName();

	if(sContainer == "Inventory")
		return gpBase->mpInventory->GetActivePlayer();

	//Unconditional. The show-on-both flag decides WHERE the journal draws, not
	//who may walk around -- tying free roam to it meant a journal that drew on one
	//half could still report "no owner" and freeze the other player. It was also
	//the only thing the journal had that the inventory did not, which is exactly
	//the difference in the bug.
	if(sContainer == "Journal")
		return gpBase->mpJournal->GetActivePlayer();

	return NULL;
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateCoopFreeRoamInput()
{
	if(gpBase->mpMapHandler->GetCoopMode()==false) return;
	if(gpBase->mpPlayer2 == NULL) return;

	//Both of them are reading the same page and the world is stopped for it.
	//Nobody is out there to drive.
	if(CoopMenuIsShared()) return;

	cLuxPlayer *pMenuOwner = GetCoopMenuOwner();

	if(pMenuOwner == NULL) return;

	//The menu owner's device drives the UI; the other one keeps playing. Gated on
	//what ACTUALLY ran this frame rather than on mState -- the menus move mState
	//around while they fade, so inferring it from there both double-drove and
	//skipped depending on the moment.
	if(pMenuOwner->IsPlayer2())
	{
		if(mbP1GameInputRan==false) UpdateGameInput();
	}
	else
	{
		if(mbP2GameInputRan==false) UpdatePlayer2Input();
	}
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::P1ActionLevel(int alAction)
{
	cAction *pAction = mpInput->GetAction(alAction);
	if(pAction==NULL) return false;

	void *pP2Device = ::ImGuiDebugMenu::GetP2RawDevice();
	cRawInputWin32 *pRaw = cRawInputWin32::GetInstance();
	if(pRaw==NULL) return false;

	for(size_t i=0; i<pAction->GetSubActionNum(); ++i)
	{
		iSubAction *pSubAction = pAction->GetSubAction(i);
		if(pSubAction==NULL) continue;

		const tString sType = pSubAction->GetInputType();

		if(sType == "Keyboard")
		{
			cActionKeyboard *pKey = static_cast<cActionKeyboard*>(pSubAction);
			if(pRaw->KeyIsDownExcludingDevice(pP2Device, pKey->GetKey())) return true;
		}
		else if(sType == "MouseButton")
		{
			cActionMouseButton *pButton = static_cast<cActionMouseButton*>(pSubAction);

			//eMouseButton and the raw button order agree for the three that raw
			//input reports as buttons: left 0, middle 1, right 2. The wheel is not
			//a button there and is left to SDL, which is where it is read anyway.
			//Excluding Player 2's MOUSE. Excluding their keyboard instead excluded
			//nothing at all, so every one of Player 2's clicks also fired Player 1's
			//interact and attack.
			//
			//And while we do not yet KNOW which mouse is theirs, SDL: excluding a
			//handle that has never sent a mouse event excludes nothing either, but
			//it also makes Player 2 holding a button read as Player 1 holding it --
			//no new edge, no clicks for Player 1 at all. Sharing the mouse for the
			//few seconds pairing takes is the lesser of those two.
			const int lButton = (int)pButton->GetButton();
			if(lButton > 2) continue;

			if(P2RawMouseKnown()==false)
			{
				if(pSubAction->IsTriggerd()) return true;
				continue;
			}

			if(pRaw->ButtonIsDownExcludingDevice(::ImGuiDebugMenu::GetP2RawMouse(), lButton)) return true;
		}
		else
		{
			//Gamepad. Player 2 is on a keyboard in this mode, so the pad is nobody
			//else's and belongs to Player 1 as it does in single player.
			if(pSubAction->IsTriggerd()) return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateP1ActionFilter()
{
	cRawInputWin32 *pRaw = cRawInputWin32::GetInstance();

	mbP1ActionFilterActive = ::ImGuiDebugMenu::GetP2UsesRawInput() &&
							pRaw != NULL && pRaw->IsAvailable() &&
							gpBase->mpMapHandler->GetCoopMode();

	if(mbP1ActionFilterActive==false) return;

	//One pass a tick, so an action asked about five times costs one walk of its
	//sub-actions rather than five.
	for(int i=0; i<(int)eLuxAction_LastEnum; ++i)
	{
		const bool bDown = P1ActionLevel(i);

		mvP1ActionBecame[i]   = bDown && mvP1ActionDown[i]==false;
		mvP1ActionReleased[i] = bDown==false && mvP1ActionDown[i];
		mvP1ActionDown[i]     = bDown;
	}
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::P1IsTriggerd(int alAction)
{
	if(mbP1ActionFilterActive==false) return mpInput->IsTriggerd(alAction);
	return mvP1ActionDown[alAction];
}

bool cLuxInputHandler::P1BecameTriggerd(int alAction)
{
	if(mbP1ActionFilterActive==false) return mpInput->BecameTriggerd(alAction);
	return mvP1ActionBecame[alAction];
}

bool cLuxInputHandler::P1WasTriggerd(int alAction)
{
	if(mbP1ActionFilterActive==false) return mpInput->WasTriggerd(alAction);
	return mvP1ActionReleased[alAction];
}

//-----------------------------------------------------------------------

void* cLuxInputHandler::GetP2RawDevice()
{
	if(::ImGuiDebugMenu::GetP2UsesRawInput()==false) return NULL;

	cRawInputWin32 *pRaw = cRawInputWin32::GetInstance();
	if(pRaw==NULL || pRaw->IsAvailable()==false) return NULL;

	//NULL means "no second device"; the injected handle is zero and IS a real
	//answer, so the two are told apart by asking the option, never by the value.
	return ::ImGuiDebugMenu::GetP2RawDevice();
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::P1KeyIsDown(eKey aKey)
{
	cRawInputWin32 *pRaw = cRawInputWin32::GetInstance();

	//Same condition the action filter uses, and set by it earlier this tick, so
	//the two can never disagree about whether Player 2 owns a device.
	if(mbP1ActionFilterActive && pRaw)
	{
		////////////////////////////////////////////////////////////////////
		// NOT SDL, once a second keyboard exists.
		//
		// SDL keeps ONE logical key state for the whole machine, so with two
		// keyboards it is simply wrong -- Player 2 holds W, Player 1 taps and
		// releases W, and SDL now says W is up while a finger is still on it.
		// Worse the other way round: Player 2 walking forward walks Player 1
		// forward too, because SDL cannot say who pressed it.
		//
		// Raw input knows per device, so Player 1 is "any device that is not
		// Player 2's" -- which also keeps a third keyboard working for them.
		return pRaw->KeyIsDownExcludingDevice(::ImGuiDebugMenu::GetP2RawDevice(), aKey);
	}

	//Nothing to separate: SDL as always.
	return mpInput->GetKeyboard()->KeyIsDown(aKey);
}

//-----------------------------------------------------------------------

void cLuxInputHandler::ResetPlayer2RawInputState()
{
	//////////////////////////////////////////////////////////////////////////
	// Seeded from the live devices, not zeroed.
	//
	// Every caller of this is a GAP in Player 2's gameplay input -- a menu of
	// theirs, a frozen player, a character body that does not exist yet -- and a
	// key that was held across the gap has to still read as held on the far side
	// of it. Zeroing meant the first frame back saw "was up, is down" for
	// anything still under a finger.
	//
	// The one that bit: an emotion stone freezes the player who touched it for
	// the length of the vision. The click that touched it is still held. The
	// moment the vision let P2 go, that same unreleased button read as a fresh
	// interact, hit the stone again, and started the whole vision over -- which
	// is exactly what "it just immediately started up for them again" is.
	cRawInputWin32 *pRaw = cRawInputWin32::GetInstance();

	if(pRaw==NULL || pRaw->IsAvailable()==false)
	{
		mbP2Raw_Jump_WasDown = false;
		mbP2Raw_Run_WasDown = false;
		mbP2Raw_Crouch_WasDown = false;
		mbP2Raw_Lantern_WasDown = false;
		mbP2Raw_Interact_WasDown = false;
		mbP2Raw_Attack_WasDown = false;
		mbP2Raw_Inventory_WasDown = false;
		mbP2Raw_Journal_WasDown = false;
		mbP2Raw_QuestLog_WasDown = false;
		mbP2Raw_RecentText_WasDown = false;
		return;
	}

	void *pDevice = ::ImGuiDebugMenu::GetP2RawDevice();
	void *pMouse  = ::ImGuiDebugMenu::GetP2RawMouse();

	//The same key and button for each one that the pass in UpdatePlayer2RawInput
	//reads. They have to agree or the seeding is worse than the zeroing.
	mbP2Raw_Jump_WasDown       = pRaw->KeyIsDownOnDevice(pDevice, eKey_Space);
	mbP2Raw_Run_WasDown        = pRaw->KeyIsDownOnDevice(pDevice, eKey_LeftShift);
	mbP2Raw_Crouch_WasDown     = pRaw->KeyIsDownOnDevice(pDevice, eKey_LeftCtrl);
	mbP2Raw_Lantern_WasDown    = pRaw->KeyIsDownOnDevice(pDevice, eKey_F);
	mbP2Raw_Interact_WasDown   = pRaw->ButtonIsDownOnDevice(pMouse, 0);
	mbP2Raw_Attack_WasDown     = pRaw->ButtonIsDownOnDevice(pMouse, 2);
	mbP2Raw_Inventory_WasDown  = pRaw->KeyIsDownOnDevice(pDevice, eKey_Tab);
	mbP2Raw_Journal_WasDown    = pRaw->KeyIsDownOnDevice(pDevice, eKey_J);
	mbP2Raw_QuestLog_WasDown   = pRaw->KeyIsDownOnDevice(pDevice, eKey_M);
	mbP2Raw_RecentText_WasDown = pRaw->KeyIsDownOnDevice(pDevice, eKey_N);
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdatePlayer2EffectInput(bool abPrimaryDown, bool abSecondaryDown)
{
	cLuxPlayer *pP2 = gpBase->mpPlayer2;

	//Only while an effect actually has somebody paused. Outside that these two
	//buttons mean what they always mean and are read by the ordinary pass.
	//cLuxEffectHandler::DoActionForPlayer does the rest of the filtering: an
	//effect that belongs to Player 1 ignores this entirely.
	if(pP2 && gpBase->mpEffectHandler->GetPlayerIsPaused())
	{
		if(abPrimaryDown && mbP2Fx_Primary_WasDown==false)
			gpBase->mpEffectHandler->DoActionForPlayer(pP2, eLuxPlayerAction_Interact, true);
		if(abPrimaryDown==false && mbP2Fx_Primary_WasDown)
			gpBase->mpEffectHandler->DoActionForPlayer(pP2, eLuxPlayerAction_Interact, false);

		if(abSecondaryDown && mbP2Fx_Secondary_WasDown==false)
			gpBase->mpEffectHandler->DoActionForPlayer(pP2, eLuxPlayerAction_Attack, true);
		if(abSecondaryDown==false && mbP2Fx_Secondary_WasDown)
			gpBase->mpEffectHandler->DoActionForPlayer(pP2, eLuxPlayerAction_Attack, false);
	}

	//Tracked every frame, paused or not -- see the note on the members. The click
	//that STARTS a vision is still held when the vision begins, so a tracker that
	//only ran during the pause would take that held button for the press that
	//skips it and the text would be gone before it was ever drawn.
	mbP2Fx_Primary_WasDown = abPrimaryDown;
	mbP2Fx_Secondary_WasDown = abSecondaryDown;
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::UpdatePlayer2RawInput()
{
	if(::ImGuiDebugMenu::GetP2UsesRawInput()==false) return false;

	cRawInputWin32 *pRaw = cRawInputWin32::GetInstance();
	if(pRaw==NULL || pRaw->IsAvailable()==false) return false;

	if(gpBase->mpMapHandler->GetCoopMode()==false) return false;

	cLuxPlayer *pP2 = gpBase->mpPlayer2;
	if(pP2==NULL || pP2->GetCharacterBody()==NULL)
	{
		ResetPlayer2RawInputState();
		return false;
	}

	//P2's keyboard is driving a menu, not their character -- either because the
	//menu is theirs, or because it is a note the pair is reading together and
	//neither of them is playing.
	{
		cLuxPlayer *pMenuOwner = GetCoopMenuOwner();
		if(CoopMenuIsShared() || (pMenuOwner && pMenuOwner->IsPlayer2()))
		{
			ResetPlayer2RawInputState();
			return true;
		}
	}

	//Two handles, because they are two devices. Keys are asked of the keyboard,
	//motion and buttons of the mouse.
	void *pDevice = ::ImGuiDebugMenu::GetP2RawDevice();
	void *pMouse  = ::ImGuiDebugMenu::GetP2RawMouse();

	//////////////////////////////////////////
	// Effects that pause the player they belong to -- the emotion-stone vision.
	//
	// Read HERE, ahead of every early-out below, because that is the whole
	// problem: the vision calls SetActive(false) on the player who set it off, so
	// for the entire length of their own vision P2 fell straight through the
	// IsActive() gate and no button of theirs was read at all. They could not
	// press through their own text; only P1 could, off a screen P1 could not see.
	//
	// The same two buttons the ordinary pass uses: left interacts, right throws.
	UpdatePlayer2EffectInput(pRaw->ButtonIsDownOnDevice(pMouse, 0),
							 pRaw->ButtonIsDownOnDevice(pMouse, 2));

	if(pP2->IsDead())
	{
		//Shared death: let P2 press through the death text too.
		bool bDeadKey = pRaw->KeyIsDownOnDevice(pDevice, eKey_Space) ||
						pRaw->ButtonIsDownOnDevice(pMouse, 0);

		if(bDeadKey && mbP2Raw_Jump_WasDown==false)
			gpBase->mpPlayer->GetHelperDeath()->OnPressButton();

		mbP2Raw_Jump_WasDown = bDeadKey;
		return true;
	}

	if(pP2->IsActive()==false)
	{
		//Reseeded rather than left stale, the same reason the pad path does it: a
		//key held across an inactive stretch would otherwise read as a fresh press
		//on the first frame P2 comes back, and for crouch that is a toggle nobody
		//asked for.
		ResetPlayer2RawInputState();
		return true;
	}

	mbP2GameInputRan = true;

	//////////////////////////////////////////
	// Move
	//
	// Player 1's layout, deliberately. Both players sit at an ordinary keyboard
	// and expect WASD to walk; giving Player 2 a different set would be a thing
	// to learn for no reason. Their device is the only thing that differs.
	if(pRaw->KeyIsDownOnDevice(pDevice, eKey_W)) pP2->Move(eCharDir_Forward, 1);
	if(pRaw->KeyIsDownOnDevice(pDevice, eKey_S)) pP2->Move(eCharDir_Forward, -1);
	if(pRaw->KeyIsDownOnDevice(pDevice, eKey_D)) pP2->Move(eCharDir_Right, 1);
	if(pRaw->KeyIsDownOnDevice(pDevice, eKey_A)) pP2->Move(eCharDir_Right, -1);

	//////////////////////////////////////////
	// Look
	//
	// Raw counts through the same scaling Player 1's mouse gets, so both players
	// turn at the same rate for the same hand movement. Sensitivity and invert are
	// shared: they are one option each and P2 has no options screen of their own.
	int lRelX = 0, lRelY = 0;
	pRaw->GetRelMotionForDevice(pMouse, &lRelX, &lRelY);

	if(lRelX != 0 || lRelY != 0)
	{
		cVector2f vRel = cVector2f((float)lRelX, (float)lRelY) * mfMouseSensitivity;
		vRel = vRel / (1.7f * mpGraphics->GetLowLevel()->GetScreenSizeFloat().y);

		if(mbInvertMouse) vRel.y = -vRel.y;

		//Free lean, on P2's own Left Alt. Player 1's co-op binding exactly -- see
		//the eKey_LeftAlt read in UpdateGamePlayerInput. Added alongside the turn
		//rather than instead of it, again because that is what P1 does.
		if(pRaw->KeyIsDownOnDevice(pDevice, eKey_LeftAlt))
			pP2->AddLean(vRel.x);

		pP2->AddYaw(-vRel.x);
		pP2->AddPitch(-vRel.y);
	}

	//////////////////////////////////////////
	// Lean
	//
	// Q and E, the same two keys and the same calls Player 1 gets in
	// UpdateGamePlayerInput. Neither path zeroes the lean when the key comes up;
	// cLuxPlayer decays it on its own, which is why P1 has never needed one.
	if(pRaw->KeyIsDownOnDevice(pDevice, eKey_E))			pP2->SetLean(1);
	else if(pRaw->KeyIsDownOnDevice(pDevice, eKey_Q))	pP2->SetLean(-1);

	//////////////////////////////////////////
	// Pressed. Level state in, edges kept here.
	//
	// Run included: cLuxPlayer::Run takes a press and a release, exactly like the
	// pad's left trigger does, rather than a level "is running" flag.
	const bool bRun = pRaw->KeyIsDownOnDevice(pDevice, eKey_LeftShift);
	if(bRun && mbP2Raw_Run_WasDown==false)	pP2->Run(true);
	if(bRun==false && mbP2Raw_Run_WasDown)	pP2->Run(false);
	mbP2Raw_Run_WasDown = bRun;

	const bool bJump = pRaw->KeyIsDownOnDevice(pDevice, eKey_Space);
	if(bJump && mbP2Raw_Jump_WasDown==false)	pP2->Jump(true);
	if(bJump==false && mbP2Raw_Jump_WasDown)	pP2->Jump(false);
	mbP2Raw_Jump_WasDown = bJump;

	const bool bCrouch = pRaw->KeyIsDownOnDevice(pDevice, eKey_LeftCtrl);
	if(bCrouch && mbP2Raw_Crouch_WasDown==false)	pP2->Crouch(true);
	if(bCrouch==false && mbP2Raw_Crouch_WasDown)	pP2->Crouch(false);
	mbP2Raw_Crouch_WasDown = bCrouch;

	const bool bLantern = pRaw->KeyIsDownOnDevice(pDevice, eKey_F);
	if(bLantern && mbP2Raw_Lantern_WasDown==false)	pP2->DoAction(eLuxPlayerAction_Lantern, true);
	if(bLantern==false && mbP2Raw_Lantern_WasDown)	pP2->DoAction(eLuxPlayerAction_Lantern, false);
	mbP2Raw_Lantern_WasDown = bLantern;

	//Left mouse interacts, right mouse throws -- the same two the default bindings
	//give Player 1 (eLuxAction_Interact and eLuxAction_Attack).
	const bool bInteract = pRaw->ButtonIsDownOnDevice(pMouse, 0);
	if(bInteract && mbP2Raw_Interact_WasDown==false)	pP2->DoAction(eLuxPlayerAction_Interact, true);
	if(bInteract==false && mbP2Raw_Interact_WasDown)	pP2->DoAction(eLuxPlayerAction_Interact, false);
	mbP2Raw_Interact_WasDown = bInteract;

	const bool bAttack = pRaw->ButtonIsDownOnDevice(pMouse, 2);
	if(bAttack && mbP2Raw_Attack_WasDown==false)	pP2->DoAction(eLuxPlayerAction_Attack, true);
	if(bAttack==false && mbP2Raw_Attack_WasDown)	pP2->DoAction(eLuxPlayerAction_Attack, false);
	mbP2Raw_Attack_WasDown = bAttack;

	//////////////////////////////////////////
	// Bag and journal.
	//
	// Player 1's default keys, for the same reason the movement keys are: both
	// players sit at an ordinary keyboard and Tab is where a bag lives.
	//
	// The OPEN is gated the way the pad's Back button is: there is one
	// cLuxInventory with one active player, so opening while a menu is already up
	// would hand the other player's open menu to P2 mid-frame, and they would read
	// that as their own closing. The edge is tracked outside the gate, so a key
	// held while the other player closes theirs cannot fire a phantom open on the
	// frame after.
	const bool bOpenInv = pRaw->KeyIsDownOnDevice(pDevice, eKey_Tab);
	if(bOpenInv && mbP2Raw_Inventory_WasDown==false)
	{
		if(GetCoopMenuOwner()==NULL && CoopMenuIsShared()==false &&
			gpBase->mpInventory->GetDisabled()==false)
		{
			gpBase->mpInventory->SetActivePlayer(pP2);
			gpBase->mpEngine->GetUpdater()->SetContainer("Inventory");
		}
	}
	mbP2Raw_Inventory_WasDown = bOpenInv;

	//All three of Player 1's journal keys, on Player 2's own keyboard: J opens it,
	//M goes straight to the quest log, N re-opens the last thing read. P2 had only
	//ever had the pad's Y button, which is why N did nothing at all for them.
	const bool bMenuFree = (GetCoopMenuOwner()==NULL && CoopMenuIsShared()==false &&
							gpBase->mpInventory->GetDisabled()==false);

	const bool bOpenJournal = pRaw->KeyIsDownOnDevice(pDevice, eKey_J);
	if(bOpenJournal && mbP2Raw_Journal_WasDown==false && bMenuFree)
	{
		//Opened FROM the journal, never from the world: this one is P2's own
		//business. It does not stop P1 and it does not draw on their half.
		gpBase->mpJournal->SetActivePlayer(pP2);
		gpBase->mpJournal->SetShowOnBothPlayers(false);
		gpBase->mpJournal->SetPauseBothPlayers(false);
		gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
	}
	mbP2Raw_Journal_WasDown = bOpenJournal;

	const bool bOpenQuest = pRaw->KeyIsDownOnDevice(pDevice, eKey_M);
	if(bOpenQuest && mbP2Raw_QuestLog_WasDown==false && bMenuFree)
	{
		gpBase->mpJournal->SetActivePlayer(pP2);
		gpBase->mpJournal->SetShowOnBothPlayers(false);
		gpBase->mpJournal->SetPauseBothPlayers(false);
		gpBase->mpJournal->SetForceInstantExit(true);
		gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
		gpBase->mpJournal->ChangeState(eLuxJournalState_QuestLog);
	}
	mbP2Raw_QuestLog_WasDown = bOpenQuest;

	const bool bOpenRecent = pRaw->KeyIsDownOnDevice(pDevice, eKey_N);
	if(bOpenRecent && mbP2Raw_RecentText_WasDown==false && bMenuFree)
	{
		gpBase->mpJournal->SetActivePlayer(pP2);
		gpBase->mpJournal->SetShowOnBothPlayers(false);
		gpBase->mpJournal->SetPauseBothPlayers(false);
		gpBase->mpJournal->OpenLastReadText();
	}
	mbP2Raw_RecentText_WasDown = bOpenRecent;

	return true;
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdatePlayer2Input()
{
	//A second keyboard and mouse instead of the pad. Returns false when that is
	//switched off or unavailable, and the gamepad path below runs as it always has.
	if(UpdatePlayer2RawInput()) return;

	//Coop: P2's pad is driving a menu, not their character. Same two cases as the
	//raw path above.
	{
		cLuxPlayer *pMenuOwner = GetCoopMenuOwner();
		if(CoopMenuIsShared() || (pMenuOwner && pMenuOwner->IsPlayer2())) return;
	}

#ifdef USE_GAMEPAD
	// Every early-out below reseeds the edge trackers from the live pad state.
	// Previously they were left stale, so a button held across an inactive stretch
	// (P2 not yet spawned, P2 deactivated by a script) read as a fresh press on the
	// very next frame P2 came back -- which for Crouch means an extra
	// SetCrouch toggle out of nowhere.
	if(!gpBase->mpMapHandler->GetCoopMode() || !IsGamepadPresent()) { ResetPlayer2InputState(); return; }

	cLuxPlayer *pP2 = gpBase->mpPlayer2;
	if(!pP2 || !pP2->GetCharacterBody()) { ResetPlayer2InputState(); return; }

	// Effects that pause the player they belong to, before the IsActive() gate --
	// see the matching pass in UpdatePlayer2RawInput for why it has to be here.
#if USE_SDL2
	UpdatePlayer2EffectInput(mpPad->ButtonIsDown(eGamepadButton_A),
							 mpPad->ButtonIsDown(eGamepadButton_B));
#else
	UpdatePlayer2EffectInput(mpPad->ButtonIsDown(eGamepadButton_0),
							 mpPad->ButtonIsDown(eGamepadButton_1));
#endif

	if(pP2->IsDead())
	{
		// Shared death: let P2's buttons skip the death text too.
#if USE_SDL2
		bool bDeadA = mpPad->ButtonIsDown(eGamepadButton_A);
#else
		bool bDeadA = mpPad->ButtonIsDown(eGamepadButton_0);
#endif
		if(bDeadA && !mbP2_A_WasDown) gpBase->mpPlayer->GetHelperDeath()->OnPressButton();
		mbP2_A_WasDown = bDeadA;
		return;
	}
	if(!pP2->IsActive()) { ResetPlayer2InputState(); return; }

	mbP2GameInputRan = true;

	//////////////////////////////////////////
	// P2 Inventory: if P2 opened the inventory (container = "Inventory"),
	// skip gameplay input — the inventory input handler takes over.
	// But this block won't run anyway since UpdatePlayer2Input only runs
	// in game state.

	float fDeadZone = mpPad->GetAxisDeadZoneRadiusValue();
	float fStepSize = gpBase->mpEngine->GetStepSize();

	//////////////////////////////////////////
	// Walk (left stick)
#if USE_SDL2
	cVector2f vWalkAxis = cVector2f(mpPad->GetAxisValue(eGamepadAxis_LeftX), mpPad->GetAxisValue(eGamepadAxis_LeftY));
#else
	cVector2f vWalkAxis = cVector2f(mpPad->GetAxisValue(eGamepadAxis_0), mpPad->GetAxisValue(eGamepadAxis_1));
#endif

	// Dead zone
	if(cMath::Abs(vWalkAxis.x) < fDeadZone) vWalkAxis.x = 0;
	else vWalkAxis.x = (vWalkAxis.x - cMath::Sign(vWalkAxis.x) * fDeadZone) / (1.0f - fDeadZone);

	if(cMath::Abs(vWalkAxis.y) < fDeadZone) vWalkAxis.y = 0;
	else vWalkAxis.y = (vWalkAxis.y - cMath::Sign(vWalkAxis.y) * fDeadZone) / (1.0f - fDeadZone);

	if(cMath::Abs(vWalkAxis.x) > 0.01f)
		pP2->Move(eCharDir_Right, vWalkAxis.x);
	if(cMath::Abs(vWalkAxis.y) > 0.01f)
		pP2->Move(eCharDir_Forward, -vWalkAxis.y);

	//////////////////////////////////////////
	// Look (right stick)
#if USE_SDL2
	cVector2f vLookAxis = cVector2f(mpPad->GetAxisValue(eGamepadAxis_RightX), mpPad->GetAxisValue(eGamepadAxis_RightY));
#else
	cVector2f vLookAxis = cVector2f(mpPad->GetAxisValue(eGamepadAxis_4), mpPad->GetAxisValue(eGamepadAxis_3));
#endif

	// Lean (hold Right Trigger, tilt with right stick X): P2 leans instead of turning.
#if USE_SDL2
	if(mpPad->GetAxisValue(eGamepadAxis_RightTrigger) > 0.5f)
	{
		float fP2LeanX = mpPad->GetAxisValue(eGamepadAxis_RightX);
		pP2->SetLean(cMath::Abs(fP2LeanX) > 0.15f ? fP2LeanX : 0.0f);
		vLookAxis = 0;
	}
	else
	{
		pP2->SetLean(0);
	}
#endif

	// Dead zone
	vLookAxis -= cVector2f(cMath::Sign(vLookAxis.x), cMath::Sign(vLookAxis.y)) * fDeadZone;
	vLookAxis *= 1.0f / (1.0f - fDeadZone);

	// Response curve
	cVector2f vExp = cMath::Vector2Abs(vLookAxis);
	vExp.x = sqrtf(vExp.x); vExp.y = sqrtf(vExp.y);
	cVector2f vGamepadLook = (vLookAxis * vExp) * mfGamepadLookSensitivity * fStepSize;

	if(mbGamepadLookInvert)
		vGamepadLook.y = -vGamepadLook.y;

	pP2->AddYaw(-vGamepadLook.x);
	pP2->AddPitch(-vGamepadLook.y);

	//////////////////////////////////////////
	// Buttons — edge detection via members (reset on every input-state change
	// in ResetPlayer2InputState() so a paused menu can't leave stale edges).
#if USE_SDL2
	bool bA = mpPad->ButtonIsDown(eGamepadButton_A);
	bool bB = mpPad->ButtonIsDown(eGamepadButton_B);
	bool bX = mpPad->ButtonIsDown(eGamepadButton_X);
	bool bLS = mpPad->ButtonIsDown(eGamepadButton_LeftShoulder);
	bool bRS = mpPad->ButtonIsDown(eGamepadButton_RightShoulder);
	bool bY  = mpPad->ButtonIsDown(eGamepadButton_Y);
#else
	bool bA = mpPad->ButtonIsDown(eGamepadButton_0);
	bool bB = mpPad->ButtonIsDown(eGamepadButton_1);
	bool bX = mpPad->ButtonIsDown(eGamepadButton_2);
	bool bLS = mpPad->ButtonIsDown(eGamepadButton_4);
	bool bRS = mpPad->ButtonIsDown(eGamepadButton_5);
	bool bY  = mpPad->ButtonIsDown(eGamepadButton_3);
#endif

	// Jump (A)
	if(bA && !mbP2_A_WasDown) pP2->Jump(true);
	if(!bA && mbP2_A_WasDown) pP2->Jump(false);
	mbP2_A_WasDown = bA;

	// Crouch (B)
	if(bB && !mbP2_B_WasDown) pP2->Crouch(true);
	if(!bB && mbP2_B_WasDown) pP2->Crouch(false);
	mbP2_B_WasDown = bB;

	// Lantern (X)
	if(bX && !mbP2_X_WasDown) pP2->DoAction(eLuxPlayerAction_Lantern, true);
	if(!bX && mbP2_X_WasDown) pP2->DoAction(eLuxPlayerAction_Lantern, false);
	mbP2_X_WasDown = bX;

	// Attack / throw (Left Shoulder). Nothing shares it -- the gun used to and
	// ate the throw whenever debug guns were on.
	if(bLS && !mbP2_LS_WasDown) pP2->DoAction(eLuxPlayerAction_Attack, true);
	if(!bLS && mbP2_LS_WasDown) pP2->DoAction(eLuxPlayerAction_Attack, false);
	mbP2_LS_WasDown = bLS;

	// Raise / stow the debug gun (Y). The only free face button: A is jump, B is
	// crouch, X is the lantern, and both triggers are run and fire.
	if(::ImGuiDebugMenu::GetDebugGuns())
	{
		if(bY && !mbP2_Y_WasDown) pP2->DoAction(eLuxPlayerAction_DebugGun, true);
	}
	mbP2_Y_WasDown = bY;

	//Right trigger fires. Edge tracked against a threshold because it is an
	//axis, not a button, so "held" would otherwise be a shot every frame.
#if USE_SDL2
	bool bTrigger = mpPad->GetAxisValue(eGamepadAxis_RightTrigger) > 0.5f;
#else
	bool bTrigger = false;
#endif
	if(bTrigger && !mbP2_GunTrigger_WasDown && pP2->GetHelperGun())
		pP2->GetHelperGun()->Fire();
	mbP2_GunTrigger_WasDown = bTrigger;

	// Interact (Right Shoulder)
	if(bRS && !mbP2_RS_WasDown) pP2->DoAction(eLuxPlayerAction_Interact, true);
	if(!bRS && mbP2_RS_WasDown) pP2->DoAction(eLuxPlayerAction_Interact, false);
	mbP2_RS_WasDown = bRS;

	//////////////////////////////////////////
	// Run (Left Trigger)
#if USE_SDL2
	float fRunAxis = mpPad->GetAxisValue(eGamepadAxis_LeftTrigger);
#else
	float fRunAxis = 0;
#endif
	bool bRunDown = fRunAxis > 0.5f;
	if(bRunDown && !mbP2_Run_WasDown) pP2->Run(true);
	if(!bRunDown && mbP2_Run_WasDown) pP2->Run(false);
	mbP2_Run_WasDown = bRunDown;

	//////////////////////////////////////////
	// Scroll (D-pad up/down)
#if USE_SDL2
	bool bDpadUp = mpPad->ButtonIsDown(eGamepadButton_DpadUp);
	bool bDpadDown = mpPad->ButtonIsDown(eGamepadButton_DpadDown);
#else
	bool bDpadUp = mpPad->HatIsInState(eGamepadHat_0, eGamepadHatState_Up);
	bool bDpadDown = mpPad->HatIsInState(eGamepadHat_0, eGamepadHatState_Down);
#endif
	if(bDpadUp) pP2->Scroll(gpBase->mpEngine->GetFrameTime() * 8.0f);
	if(bDpadDown) pP2->Scroll(-gpBase->mpEngine->GetFrameTime() * 8.0f);

	//////////////////////////////////////////
	// Inventory (Back/Select button)
#if USE_SDL2
	bool bBack = mpPad->ButtonIsDown(eGamepadButton_Back);
#else
	bool bBack = mpPad->ButtonIsDown(eGamepadButton_6);
#endif
	if(bBack && !mbP2_Back_WasDown)
	{
		//Coop: this function keeps running while P1 has THEIR bag open -- that is
		//what lets P2 carry on playing. But there is one cLuxInventory with one
		//active player, so opening here would hand P1's open bag to P2 mid-frame.
		//P1 reads that as P2's button closing their inventory.
		//
		//UpdateGameInput() already refuses the mirror case (bOpenP1 is forced false
		//outside eLuxInputState_Game); this is the same guard for P2's side.
		//
		//Only the OPEN is gated. mbP2_Back_WasDown below still tracks the edge, so
		//holding Back while P1 closes their bag cannot fire a phantom open on the
		//frame after.
		if(GetCoopMenuOwner() == NULL && gpBase->mpInventory->GetDisabled()==false)
		{
			gpBase->mpInventory->SetActivePlayer(pP2);
			gpBase->mpEngine->GetUpdater()->SetContainer("Inventory");
		}
	}
	mbP2_Back_WasDown = bBack;

	//////////////////////////////////////////
	// Debug teleport: pressing BOTH stick buttons teleports P2 to P1.
#if USE_SDL2
	bool bStickL = mpPad->ButtonIsDown(eGamepadButton_LeftStick);
	bool bStickR = mpPad->ButtonIsDown(eGamepadButton_RightStick);
#else
	bool bStickL = mpPad->ButtonIsDown(eGamepadButton_8);
	bool bStickR = mpPad->ButtonIsDown(eGamepadButton_9);
#endif
	bool bBothSticks = bStickL && bStickR;
	if(bBothSticks && !mbP2_BothSticks_WasDown && ImGuiDebugMenu::GetPlayerTeleport())
	{
		gpBase->mpMapHandler->TeleportPlayerToOther(pP2, gpBase->mpPlayer);
	}
	mbP2_BothSticks_WasDown = bBothSticks;

#endif // USE_GAMEPAD
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateGameMessageInput()
{
	//Attack
	if(mpInput->BecameTriggerd(eLuxAction_Attack))	gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Attack, true);
	if(mpInput->WasTriggerd(eLuxAction_Attack))		gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Attack, false);

	//Interact
	if(mpInput->BecameTriggerd(eLuxAction_Interact))gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Interact ,true);
	if(mpInput->WasTriggerd(eLuxAction_Interact))	gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Interact, false);

#ifdef USE_GAMEPAD
	////////////
	// Use the UI input from the gamepad to do the same thing
	if(mpInput->BecameTriggerd(eLuxAction_UIPrimary))	gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Interact, true);
	if(mpInput->WasTriggerd(eLuxAction_UIPrimary))		gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Interact, false);

	if(mpInput->BecameTriggerd(eLuxAction_UISecondary))	gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Attack, true);
	if(mpInput->WasTriggerd(eLuxAction_UISecondary))	gpBase->mpMessageHandler->DoAction(eLuxPlayerAction_Attack, false);
#endif
	
}

//-----------------------------------------------------------------------

void cLuxInputHandler:: UpdateGameEffectInput()
{
	//Coop: this is PLAYER 1 pressing, and it is said so rather than left
	//anonymous. cLuxEffectHandler::GetPlayerIsPaused is one global bool meaning
	//"somebody is paused", so this function runs whichever player an effect
	//belongs to -- and an emotion-stone vision belongs to whoever touched the
	//stone, is drawn on their half only, and froze them alone. Unattributed, P1's
	//ordinary interact click was skipping pages of text off P2's screen that P1
	//could not see, while P2 -- frozen, and so never reaching a button read at
	//all -- had no way out of their own vision.
	cLuxPlayer *pP1 = gpBase->mpPlayer;

	//Attack
	if(mpInput->BecameTriggerd(eLuxAction_Attack))	gpBase->mpEffectHandler->DoActionForPlayer(pP1, eLuxPlayerAction_Attack, true);
	if(mpInput->WasTriggerd(eLuxAction_Attack))		gpBase->mpEffectHandler->DoActionForPlayer(pP1, eLuxPlayerAction_Attack, false);

	//Interact
	if(mpInput->BecameTriggerd(eLuxAction_Interact))gpBase->mpEffectHandler->DoActionForPlayer(pP1, eLuxPlayerAction_Interact ,true);
	if(mpInput->WasTriggerd(eLuxAction_Interact))	gpBase->mpEffectHandler->DoActionForPlayer(pP1, eLuxPlayerAction_Interact, false);

#ifdef USE_GAMEPAD
	////////////
	// Use the UI input from the gamepad to do the same thing
	if(mpInput->BecameTriggerd(eLuxAction_UIPrimary))	gpBase->mpEffectHandler->DoActionForPlayer(pP1, eLuxPlayerAction_Interact, true);
	if(mpInput->WasTriggerd(eLuxAction_UIPrimary))		gpBase->mpEffectHandler->DoActionForPlayer(pP1, eLuxPlayerAction_Interact, false);

	//mpEffectHandler, not mpMessageHandler. This one line went to the message
	//handler in a function that has nothing to do with it -- a copy-paste from
	//UpdateGameMessageInput directly above, and the reason a pad's B button set
	//off the press half of a message action while an effect was running.
	if(mpInput->BecameTriggerd(eLuxAction_UISecondary))	gpBase->mpEffectHandler->DoActionForPlayer(pP1, eLuxPlayerAction_Attack, true);
	if(mpInput->WasTriggerd(eLuxAction_UISecondary))	gpBase->mpEffectHandler->DoActionForPlayer(pP1, eLuxPlayerAction_Attack, false);
#endif
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateInventoryInput()
{
	//Coop: P1 nudging the mouse out in the world must not drag P2's bag back
	//into cursor mode underneath them.
	if(CoopMenuAcceptsKeyboard() && ShowMouseOnMouseInput())
	{
		gpBase->mpInventory->GetSet()->SetMouseMovementEnabled(true);
		gpBase->mpInventory->GetSet()->SetDrawMouse(true);
		gpBase->mpInventory->GetSet()->SetDrawFocus(false);
	}

	////////////////////
	//Exit
	bool bExitPressed = false;
	if(	MenuActionTriggered(eLuxAction_Exit) ||
		MenuActionTriggered(eLuxAction_Inventory))
	{
		bExitPressed = true;
	}

#ifdef USE_GAMEPAD
	// In coop mode, also check gamepad Back button directly for P2 closing inventory.
	// The unified action system may not reliably fire BecameTriggerd for the gamepad
	// button when the same button was used to open the inventory.
	if (!bExitPressed && CoopMenuAcceptsGamepad() && gpBase->mpMapHandler->GetCoopMode() && IsGamepadPresent())
	{
#if USE_SDL2
		bool bBackDown = mpPad->ButtonIsDown(eGamepadButton_Back);
#else
		bool bBackDown = mpPad->ButtonIsDown(eGamepadButton_6);
#endif
		// mbP2_InvBack_WasDown is seeded to the current Back state on entering
		// the Inventory state (ResetPlayer2InputState), so the same press that
		// opened the inventory can't immediately close it — only a fresh press.
		if (bBackDown && !mbP2_InvBack_WasDown)
			bExitPressed = true;
		mbP2_InvBack_WasDown = bBackDown;
	}
#endif

	//////////////////////
	// P2 on their own keyboard closes their own bag.
	//
	// The merged-action path above cannot do it: eLuxAction_Inventory is fed by
	// SDL, which has one keyboard for the machine, so honouring it here would let
	// P1's Tab close P2's bag from across the room. Their device, asked directly.
	if(CoopMenuAcceptsP2Raw() && CoopMenuIsShared()==false)
	{
		cRawInputWin32 *pRaw = cRawInputWin32::GetInstance();
		void *pDevice = ::ImGuiDebugMenu::GetP2RawDevice();

		bool bClose = pRaw && (	pRaw->KeyIsDownOnDevice(pDevice, eKey_Tab) ||
								pRaw->KeyIsDownOnDevice(pDevice, eKey_Escape));

		//Seeded from the live key in ResetPlayer2RawGuiState, so the Tab that
		//opened this bag cannot be the Tab that closes it.
		if(bClose && mbP2Raw_GuiInventory_WasDown==false) bExitPressed = true;
		mbP2Raw_GuiInventory_WasDown = bClose;
	}

	if (bExitPressed)
	{
		gpBase->mpInventory->ExitPressed();
	}

	////////////////////
	//Journal
	if(MenuActionTriggered(eLuxAction_Journal))
	{
		gpBase->mpInventory->OpenJournal();
	}

	////////////////////
	//If a message is active, we use special input
	if(gpBase->mpInventory->GetMessageActive())
	{
		//Coop: the cursor belongs to P1. Pushing the mouse into a bag P2 owns drags
		//their pointer around from the other side of the room.
		if(CoopMenuAcceptsKeyboard())
			gpBase->mpEngine->GetGui()->SendMousePos(GetGuiMouseAbsPos(), GetGuiMouseRelPos());

		//UIPrimary is gamepad A and UISecondary is B, so raw BecameTriggerd here let
		//P2 dismiss a pop-up in P1's bag -- and ATE the press on the way through, so
		//P2's jump never reached UpdatePlayer2Input either.
		if(	MenuActionTriggered(eLuxAction_LeftClick) || MenuActionTriggered(eLuxAction_MiddleClick) ||
			MenuActionTriggered(eLuxAction_RightClick) ||
			MenuActionTriggered(eLuxAction_UIPrimary) ||
			MenuActionTriggered(eLuxAction_UISecondary))
		{
			gpBase->mpInventory->ExitPressed(); //This just exits message!		
		}
	}

}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdatePreMenuInput()
{
	if(ShowMouseOnMouseInput())
	{
		gpBase->mpPreMenu->GetSet()->SetMouseMovementEnabled(true);
		gpBase->mpPreMenu->GetSet()->SetDrawMouse(true);
	}
	
	////////////////////
	//Key press
	if(mpInput->BecameTriggerd(eLuxAction_Exit) || mpInput->BecameTriggerd(eLuxAction_UIPrimary))
	{
		gpBase->mpPreMenu->ButtonPressed();
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateMainMenuInput()
{
	if(ShowMouseOnMouseInput())
	{
		gpBase->mpMainMenu->GetSet()->SetMouseMovementEnabled(true);
		gpBase->mpMainMenu->GetSet()->SetDrawMouse(true);
	}

	////////////////////
	//Exit
	if(mpInput->BecameTriggerd(eLuxAction_Exit))
	{
		gpBase->mpMainMenu->ExitPressed();
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateJournalInput()
{
	//See the note in UpdateInventoryInput.
	if(CoopMenuAcceptsKeyboard() && ShowMouseOnMouseInput())
	{
		gpBase->mpJournal->GetSet()->SetMouseMovementEnabled(true);
		gpBase->mpJournal->GetSet()->SetDrawMouse(true);
	}

	////////////////////
	//Exit
	//Coop: only the owner's device closes it.
	if(	MenuActionTriggered(eLuxAction_Exit))
	{
		gpBase->mpJournal->ExitPressed(false);
	}
	else if(MenuActionTriggered(eLuxAction_Journal) || 
			MenuActionTriggered(eLuxAction_QuestLog) || 
			MenuActionTriggered(eLuxAction_Inventory) ||
			MenuActionTriggered(eLuxAction_RecentText))
	{
#ifdef USE_GAMEPAD
		if(mbGamepadUIInput==false) gpBase->mpJournal->ExitPressed(true);
#else
		gpBase->mpJournal->ExitPressed(true);
#endif
	}

#ifdef USE_GAMEPAD
	////////////////////
	// P2 owns this journal, so Back closes it -- they have no keyboard, and the
	// merged-action path above is deliberately shut for them.
	if(CoopMenuAcceptsGamepad() && gpBase->mpMapHandler->GetCoopMode() && IsGamepadPresent())
	{
#if USE_SDL2
		bool bBackDown = mpPad->ButtonIsDown(eGamepadButton_Back);
#else
		bool bBackDown = mpPad->ButtonIsDown(eGamepadButton_6);
#endif
		if(bBackDown && !mbP2_JournalBack_WasDown) gpBase->mpJournal->ExitPressed(true);
		mbP2_JournalBack_WasDown = bBackDown;
	}
#endif

	//////////////////////
	// Player 2 on their own keyboard.
	//
	// Two different jobs behind one condition:
	//
	//   their own journal -> J or Escape closes it, the same two keys P1 has,
	//                        asked of THEIR device so P1's J cannot close it.
	//   a shared note     -> either of them may put it down, so P2's Escape ends
	//                        it for the pair. P1's already does, through the
	//                        merged action above.
	if(CoopMenuAcceptsP2Raw())
	{
		cRawInputWin32 *pRaw = cRawInputWin32::GetInstance();
		void *pDevice = ::ImGuiDebugMenu::GetP2RawDevice();

		//Every key that closes it for Player 1 closes it for Player 2, asked of
		//THEIR device. J alone was not enough: N opens the last read text and M the
		//quest log, so a note opened with N had no key that would put it down
		//again -- and Tab is the bag, which also exits a journal.
		bool bClose = pRaw && (	pRaw->KeyIsDownOnDevice(pDevice, eKey_Escape) ||
								pRaw->ButtonIsDownOnDevice(::ImGuiDebugMenu::GetP2RawMouse(), 2) ||
								(CoopMenuIsShared()==false &&
									(pRaw->KeyIsDownOnDevice(pDevice, eKey_J) ||
									 pRaw->KeyIsDownOnDevice(pDevice, eKey_N) ||
									 pRaw->KeyIsDownOnDevice(pDevice, eKey_M) ||
									 pRaw->KeyIsDownOnDevice(pDevice, eKey_Tab))));

		if(bClose && mbP2Raw_GuiJournal_WasDown==false) gpBase->mpJournal->ExitPressed(true);
		mbP2Raw_GuiJournal_WasDown = bClose;

		//////////////////////////////////////////////////////////////////
		// Turning the page on a note they are BOTH reading.
		//
		// SPACE ONLY, now that Player 2 has a pointer of their own. Their left
		// button goes to the GUI at that pointer, the same as Player 1's does at
		// theirs, so it turns the page by hitting the arrow -- and leaving the
		// blanket advance on the same button would turn TWO pages every time they
		// managed to hit it.
		//
		// Space stays as the one that does not need aim. Past the last page it
		// finishes the note for the pair, which is the whole point of a shared
		// note: whoever is done first is done for both.
		if(CoopMenuIsShared() && pRaw)
		{
			bool bAdvance =	pRaw->KeyIsDownOnDevice(pDevice, eKey_Space);

			if(bAdvance && mbP2Raw_NoteAdvance_WasDown==false)
				gpBase->mpJournal->CoopAdvanceNotePage();

			mbP2Raw_NoteAdvance_WasDown = bAdvance;
		}
		else
		{
			mbP2Raw_NoteAdvance_WasDown = false;
		}
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateDebugInput()
{
	if(mpInput->BecameTriggerd(eLuxAction_OpenDebug))
	{
		gpBase->mpDebugHandler->SetDebugWindowActive(false);
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateCreditsInput()
{
	if(	mpInput->BecameTriggerd(eLuxAction_Exit) ||
		mpInput->BecameTriggerd(eLuxAction_Jump) ||
		mpInput->BecameTriggerd(eLuxAction_Attack) ||
		mpInput->BecameTriggerd(eLuxAction_Interact) ||
		mpInput->BecameTriggerd(eLuxAction_Inventory))
	{
		gpBase->mpCredits->ExitPressed();
	}
}

//-----------------------------------------------------------------------

void cLuxInputHandler::UpdateDemoEndInput()
{
	if(gpBase->mpDemoEnd)
	{
		if(mpInput->BecameTriggerd(eLuxAction_Exit))
		{
			gpBase->mpDemoEnd->Exit(false);
		}
	}
}

//-----------------------------------------------------------------------


void cLuxInputHandler::UpdateLoadScreenInput()
{
	if(	mpInput->BecameTriggerd(eLuxAction_Exit) ||
		mpInput->BecameTriggerd(eLuxAction_Jump) ||
		mpInput->BecameTriggerd(eLuxAction_Attack) ||
		mpInput->BecameTriggerd(eLuxAction_Interact) ||
		mpInput->BecameTriggerd(eLuxAction_Inventory))
	{
		gpBase->mpLoadScreenHandler->ExitPressed();
	}
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::CurrentStateSendsInputToGui()
{
	switch(mState)
	{
	case eLuxInputState_Inventory:
		{
			//When message is active, do not send anything to GUI.
			if(gpBase->mpInventory->GetMessageActive()) return false;
		}
	case eLuxInputState_Debug: 
	case eLuxInputState_Journal:
	case eLuxInputState_MainMenu: 
	case eLuxInputState_PreMenu:
	case eLuxInputState_DemoEnd:
		return true;	
	}

	return false;
}

//-----------------------------------------------------------------------

void cLuxInputHandler::CreateActions()
{
	//////////////////////////////////
	// Loop through all defined actions and create them
	for(int i=0; gvLuxActions[i].msName != ""; ++i)
	{
		cLuxAction *pLuxAction = &gvLuxActions[i];

        mpInput->CreateAction(pLuxAction->msName, pLuxAction->mlId);
	}

	tString sSep = ".";
	//////////////////////////////////
	// Loop through all defined inputs and bind them to actions
	for(int i=0; gvLuxInputs[i].msInputType != ""; ++i)
	{
		cLuxInput *pLuxInput = &gvLuxInputs[i];

        cAction *pAction = mpInput->GetAction(pLuxInput->mlActionId);

		int lPara = 0;
		if(pLuxInput->mlActionId==eLuxAction_Forward)
			lPara=1;

		tStringVec vInputParts;
		cString::GetStringVec(pLuxInput->msInputType, vInputParts, &sSep);

		CreateSubAction(pAction, 
						vInputParts,
						pLuxInput->mlValue);
	}
    
}

//-----------------------------------------------------------------------

void cLuxInputHandler::CreateSubAction(cAction *apAction, const tStringVec& avType, int alValue)
{
	tString sType = cString::ToLowerCase(avType[0]);
	
    //Keyboard
	if(sType == "keyboard")
	{
		apAction->AddKey((eKey)alValue);
	}
	//Mouse
	else if(sType == "mousebutton")
	{
		apAction->AddMouseButton((eMouseButton)alValue);
	}
#ifdef USE_GAMEPAD
	//Gamepad
	else if(cString::GetFirstStringPos(sType, "gamepad")==0)
	{
		if(avType[0]=="GamepadButton")
			apAction->AddGamepadButton(0, (eGamepadButton)alValue);
		else
		{
			eGamepadHat hat = iGamepad::StringToHat(avType[1]);
			eGamepadAxis axis = iGamepad::StringToAxis(avType[1]);

			//////////////////////////////////////////////////
			// Gamepad elements
			if(hat!=eGamepadHat_LastEnum)
			{
				apAction->AddGamepadHat(0, hat, (eGamepadHatState)alValue);
			}
			else if(axis!=eGamepadAxis_LastEnum)
			{
				apAction->AddGamepadAxis(0, axis, (eGamepadAxisRange)alValue, 0.25f);
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------

tStringVec cLuxInputHandler::GetInputValueStrings(const tString& asX)
{
	tStringVec vInputStrings;
	cString::GetStringVec(asX, vInputStrings);

	return vInputStrings;
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::CreateSubActionFromInputString(cAction* apAction, const tString& asInputString)
{
	tString sSep = ".";
	tStringVec vInputParts;
	cString::GetStringVec(asInputString, vInputParts, &sSep);

	if(vInputParts.empty()==false)
	{
		int lInputValue = -1;
		tString sInputType = cString::ToLowerCase(vInputParts[0]);
				
		///////////////////////////////////////////////////
		// Now check if the input type - value combo is valid
		if(sInputType=="keyboard")
		{
			lInputValue = mpInput->GetKeyboard()->StringToKey(vInputParts[1]);
			if(lInputValue==eKey_LastEnum) lInputValue=-1;
		}
		else if(sInputType=="mousebutton")
		{
			lInputValue = mpInput->GetMouse()->StringToButton(vInputParts[1]);
			if(lInputValue==eMouseButton_LastEnum) lInputValue=-1;
		}
#ifdef USE_GAMEPAD
		else if(cString::GetFirstStringPos(sInputType,"gamepad")!=-1)
		{

			if(vInputParts[0]=="GamepadButton")
			{
				lInputValue = iGamepad::StringToButton(vInputParts[1]);
				if(lInputValue==eGamepadButton_LastEnum) lInputValue=-1;
			}
			else
			{
				eGamepadHat hat = iGamepad::StringToHat(vInputParts[1]);
				eGamepadAxis axis = iGamepad::StringToAxis(vInputParts[1]);
								
				if(hat!=eGamepadHat_LastEnum)
				{
					lInputValue = iGamepad::StringToHatState(vInputParts[2]);
					if(lInputValue==eGamepadHat_LastEnum) lInputValue=-1;
				}
				else if(axis!=eGamepadAxis_LastEnum)
				{
					lInputValue = iGamepad::StringToAxisRange(vInputParts[2]);
					if(lInputValue==eGamepadAxisRange_LastEnum) lInputValue=-1;
				}
			}
		}
#else
		else
			vInputParts.clear();
#endif
		
		////////////////////////////////////////
		// If input is valid, create sub action
		if(vInputParts.empty()==false && lInputValue!=-1)
		{
			CreateSubAction(apAction, vInputParts, lInputValue);

			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------

bool cLuxInputHandler::ShowMouseOnMouseInput()
{
	////////////////
	// Checks if there are have been any mouse input since last frame
	bool bMouseActive = false;

	for(int i = 0; i < eMouseButton_LastEnum; ++i)
	{
		bMouseActive = bMouseActive || mpInput->GetMouse()->ButtonIsDown(eMouseButton(i));
	}

	/////////////////
	// Get the relative mouse position, cant use GetRelPosition() when mouse cursor is hidden
	cVector2l vAbsPos = mpInput->GetMouse()->GetAbsPosition();

	bMouseActive = bMouseActive || (mvLastAbsMousePos - vAbsPos).SqrLength() > 0;

	mvLastAbsMousePos = vAbsPos;

	////////////////
	// Show the cursor
	if(bMouseActive)
	{
		if(mfMouseActiveAt != -1)
		{
		//	gpBase->SetDrawOnLiveCursor(true);
		}
		else
		{
			bMouseActive = false;
		}
		mfMouseActiveAt = gpBase->mpEngine->GetGameTime();
	}
#ifdef USE_GAMEPAD
	//COOP: this branch does not read the menu, it WRITES to it -- it switches
	//every GUI set to gamepad navigation, hides the cursor and disables mouse
	//movement. And it decides that from eLuxAction_UIArrow*, which is bound to
	//the D-pad and the LEFT STICK: the stick P2 walks with.
	//
	//So while P1 had their bag open, every step P2 took hid P1's cursor, cut
	//P1's mouse out of the GUI and turned on the focus highlight that then
	//snapped between slots. The guard at the call site cannot catch this --
	//CoopMenuAcceptsKeyboard() is TRUE precisely when P1 owns the menu, so the
	//&& never short-circuits. It has to be here, on the device.
	//
	//Also stops the five-second idle timeout below from taking P1's cursor away
	//while they are simply reading something.
	//GetP2UsesRawInput: Player 2 is on a second keyboard and MOUSE, so a pad that
	//happens to also be plugged in is nobody's navigation device. Without this the
	//idle timeout below took P2's cursor away five seconds into reading a note,
	//and there was no stick to bring it back with.
	else if(IsGamepadPresent() && CoopMenuAcceptsGamepad() &&
			::ImGuiDebugMenu::GetP2UsesRawInput()==false)
	{
		bool bDirPressed = mpInput->IsTriggerd(eLuxAction_UIArrowUp) || 
						   mpInput->IsTriggerd(eLuxAction_UIArrowDown) ||
						   mpInput->IsTriggerd(eLuxAction_UIArrowLeft) ||
						   mpInput->IsTriggerd(eLuxAction_UIArrowRight);

		if(bDirPressed || (mfMouseActiveAt + 5 < gpBase->mpEngine->GetGameTime() && mfMouseActiveAt > 0))
		{
			gpBase->mpInventory->GetSet()->SetDrawMouse(false);
			gpBase->mpInventory->GetSet()->SetMouseMovementEnabled(false);
			gpBase->mpInventory->GetSet()->SetDrawFocus(true);
			gpBase->mpMainMenu->GetSet()->SetMouseMovementEnabled(false);
			gpBase->mpMainMenu->GetSet()->SetDrawMouse(false);
			gpBase->mpJournal->GetSet()->SetMouseMovementEnabled(false);
			gpBase->mpJournal->GetSet()->SetDrawMouse(false);
			gpBase->mpPreMenu->GetSet()->SetMouseMovementEnabled(false);
			gpBase->mpPreMenu->GetSet()->SetDrawMouse(false);

			//The REAL current mouse position, not a fabricated screen-centre.
			//Writing the centre here meant the next frame compared the actual
			//position against a value nobody produced, read that as "the mouse
			//moved", re-armed mfMouseActiveAt, and re-entered this branch a frame
			//later: a self-sustaining two-frame oscillator that rewrote four GUI
			//sets' mouse/focus state every other frame for as long as a stick was
			//held. With the real position stored, only real movement reactivates.
			mvLastAbsMousePos = vAbsPos;
			mfMouseActiveAt = -1;
		}
	}
#endif

	return bMouseActive;
}

//-----------------------------------------------------------------------

#ifdef USE_GAMEPAD
void cLuxInputHandler::SetUpGamepad()
{
	mpPad = mpInput->GetGamepad(0);
	if(IsGamepadPresent())
	{
		mpPad->SetAxisDeadZoneRadiusValue(0.15f);
	}
}
#endif

//-----------------------------------------------------------------------
