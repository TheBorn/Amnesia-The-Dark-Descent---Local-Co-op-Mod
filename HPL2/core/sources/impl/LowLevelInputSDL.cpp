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

#include "impl/LowLevelInputSDL.h"

#include <impl/ImGuiManager.h>
#include "impl/ImGuiConsole.h"
#include "impl/ImGuiDebugMenu.h"

#include "impl/MouseSDL.h"
#include "impl/KeyboardSDL.h"
#include "impl/GamepadSDL.h"
#include "impl/GamepadSDL2.h"

#include "system/LowLevelSystem.h"
#include "system/Platform.h"
#include "graphics/LowLevelGraphics.h"

#include "engine/Engine.h"

#if USE_SDL2
#include "SDL2/SDL.h"
#include "SDL2/SDL_syswm.h"
#else
#include "SDL/SDL.h"
#include "SDL/SDL_syswm.h"
#endif

#if defined _WIN32 && !SDL_VERSION_ATLEAST(2,0,0)
#include <Windows.h>
#include <Dbt.h>
#endif

//Raw input needs the Windows headers on EVERY Windows build, not just the SDL1
//one the block above covers.
#if defined _WIN32
#include <Windows.h>
#if USE_SDL2
#include "SDL2/SDL_system.h"
#else
#include "SDL/SDL_system.h"
#endif
#endif

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// RAW INPUT (WINDOWS): A SECOND KEYBOARD AND MOUSE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cRawInputDeviceState::cRawInputDeviceState()
	{
		mvKeyDown.resize(eKey_LastEnum, false);

		mlRelX = 0;
		mlRelY = 0;
		mlWheel = 0;

		for(int i=0; i<8; ++i) mvButtonDown[i] = false;

		mfLastActivityTime = 0;
		mlEventCount = 0;
		mbIsKeyboard = false;
		mbIsMouse = false;

		mlRelEventCount = 0;
		mlAbsEventCount = 0;
		mbHasLastAbs = false;
		mlLastAbsX = 0;
		mlLastAbsY = 0;
	}

	//-----------------------------------------------------------------------

#if defined _WIN32

	//Windows hands us a virtual key; the engine speaks eKey. Only the keys that can
	//actually be bound to something are worth mapping -- anything missing simply
	//never reaches a player, which is the safe direction to fail in.
	static eKey RawVKeyToKey(int alVKey, bool abExtended)
	{
		//Letters and digits are contiguous in both, so they are arithmetic.
		if(alVKey >= 'A' && alVKey <= 'Z') return (eKey)(eKey_A + (alVKey - 'A'));
		if(alVKey >= '0' && alVKey <= '9') return (eKey)(eKey_0 + (alVKey - '0'));
		if(alVKey >= VK_F1 && alVKey <= VK_F15) return (eKey)(eKey_F1 + (alVKey - VK_F1));
		if(alVKey >= VK_NUMPAD0 && alVKey <= VK_NUMPAD9) return (eKey)(eKey_KP_0 + (alVKey - VK_NUMPAD0));

		switch(alVKey)
		{
		case VK_BACK:		return eKey_BackSpace;
		case VK_TAB:		return eKey_Tab;
		//The E0 prefix is the only thing separating numpad Enter from Return.
		case VK_RETURN:		return abExtended ? eKey_KP_Enter : eKey_Return;
		case VK_PAUSE:		return eKey_Pause;
		case VK_ESCAPE:		return eKey_Escape;
		case VK_SPACE:		return eKey_Space;
		case VK_DELETE:		return eKey_Delete;
		case VK_UP:			return eKey_Up;
		case VK_DOWN:		return eKey_Down;
		case VK_LEFT:		return eKey_Left;
		case VK_RIGHT:		return eKey_Right;
		case VK_INSERT:		return eKey_Insert;
		case VK_HOME:		return eKey_Home;
		case VK_END:		return eKey_End;
		case VK_PRIOR:		return eKey_PageUp;
		case VK_NEXT:		return eKey_PageDown;
		case VK_NUMLOCK:	return eKey_NumLock;
		case VK_CAPITAL:	return eKey_CapsLock;
		case VK_SCROLL:		return eKey_ScrollLock;
		case VK_LSHIFT:		return eKey_LeftShift;
		case VK_RSHIFT:		return eKey_RightShift;
		case VK_LCONTROL:	return eKey_LeftCtrl;
		case VK_RCONTROL:	return eKey_RightCtrl;
		case VK_LMENU:		return eKey_LeftAlt;
		case VK_RMENU:		return eKey_RightAlt;
		//Raw input reports the generic modifier as well as the sided one on some
		//keyboards. E0 marks the right-hand key; without it, the left.
		case VK_SHIFT:		return abExtended ? eKey_RightShift : eKey_LeftShift;
		case VK_CONTROL:	return abExtended ? eKey_RightCtrl : eKey_LeftCtrl;
		case VK_MENU:		return abExtended ? eKey_RightAlt : eKey_LeftAlt;
		case VK_LWIN:		return eKey_LeftSuper;
		case VK_RWIN:		return eKey_RightSuper;
		case VK_APPS:		return eKey_Menu;
		case VK_DECIMAL:	return eKey_KP_Period;
		case VK_DIVIDE:		return eKey_KP_Divide;
		case VK_MULTIPLY:	return eKey_KP_Multiply;
		case VK_SUBTRACT:	return eKey_KP_Minus;
		case VK_ADD:		return eKey_KP_Plus;
		case VK_OEM_1:		return eKey_SemiColon;
		case VK_OEM_PLUS:	return eKey_Equals;
		case VK_OEM_COMMA:	return eKey_Comma;
		case VK_OEM_MINUS:	return eKey_Minus;
		case VK_OEM_PERIOD:	return eKey_Period;
		case VK_OEM_2:		return eKey_Slash;
		case VK_OEM_3:		return eKey_BackQuote;
		case VK_OEM_4:		return eKey_LeftBracket;
		case VK_OEM_5:		return eKey_BackSlash;
		case VK_OEM_6:		return eKey_RightBracket;
		case VK_OEM_7:		return eKey_Quote;
		}

		return eKey_None;
	}

	//The hook is a plain C callback, so it needs a way back to the object. One
	//cLowLevelInputSDL exists at a time, and SDL_SetWindowsMessageHook takes a
	//userdata pointer anyway -- this is only here so the callback can stay static.
	static void SDLCALL gRawInputMessageHook(void *apUserData, void *apHWnd,
											unsigned int alMessage, Uint64 alWParam, Sint64 alLParam)
	{
		if(alMessage != WM_INPUT) return;

		cRawInputWin32 *pRaw = (cRawInputWin32*)apUserData;
		if(pRaw) pRaw->HandleRawInputMessage((void*)(intptr_t)alLParam);
	}

#endif //_WIN32

	//-----------------------------------------------------------------------

	cRawInputWin32* cRawInputWin32::mpInstance = NULL;

	cRawInputWin32::cRawInputWin32()
	{
		mbAvailable = false;
		mpLastActiveKeyboard = kRawInputInjectedDevice;
		mpLastActiveMouse = kRawInputInjectedDevice;

		mpInstance = this;
	}

	cRawInputWin32::~cRawInputWin32()
	{
		if(mpInstance == this) mpInstance = NULL;
	}

	//-----------------------------------------------------------------------

	void cRawInputWin32::Init()
	{
		if(mbAvailable) return;

#if defined _WIN32 && USE_SDL2
		//////////////////////////////////////////////////////////////////////
		// KEYBOARD ONLY. The mouse is deliberately NOT registered here.
		//
		// SDL2 already registers raw mouse input on Windows whenever relative
		// mouse mode is on -- ToggleRawInput() in SDL_windowsmouse.c, usage page
		// 1 usage 2 -- and this game turns relative mode on for gameplay.
		// Registration is per process per usage page, so registering the mouse
		// again here would REPLACE SDL's and break its relative motion entirely.
		//
		// It costs nothing: SDL's registration uses hwndTarget NULL, so WM_INPUT
		// follows keyboard focus to our window, and the hook below sees mouse and
		// keyboard messages alike.
		//
		// No RIDEV_NOLEGACY either. The ordinary WM_KEYDOWN stream has to keep
		// flowing or SDL -- and with it the menus, the console and all text entry
		// -- would go deaf.
		RAWINPUTDEVICE rawKeyboard;
		rawKeyboard.usUsagePage = 0x01;
		rawKeyboard.usUsage = 0x06;
		rawKeyboard.dwFlags = 0;
		rawKeyboard.hwndTarget = NULL;

		if(RegisterRawInputDevices(&rawKeyboard, 1, sizeof(RAWINPUTDEVICE)) == FALSE)
		{
			Warning("Could not register for raw keyboard input; a second keyboard cannot be told apart\n");
			return;
		}

		SDL_SetWindowsMessageHook(gRawInputMessageHook, this);

		mbAvailable = true;
		Log("Raw input started -- a second keyboard and mouse can now be told apart\n");
#endif
	}

	//-----------------------------------------------------------------------

	void cRawInputWin32::BeginFrame()
	{
		//Deltas only. Key and button state is level, and clearing it here would
		//make every held key look like a tap.
		std::map<tRawInputDevice, cRawInputDeviceState>::iterator it = mmapDevices.begin();
		for(; it != mmapDevices.end(); ++it)
		{
			it->second.mlRelX = 0;
			it->second.mlRelY = 0;
			it->second.mlWheel = 0;
		}
	}

	//-----------------------------------------------------------------------

	void cRawInputWin32::HandleRawInputMessage(void *apLParam)
	{
#if defined _WIN32 && USE_SDL2
		UINT lSize = 0;
		if(GetRawInputData((HRAWINPUT)apLParam, RID_INPUT, NULL, &lSize, sizeof(RAWINPUTHEADER)) != 0) return;
		if(lSize == 0 || lSize > 1024) return;

		BYTE vBuffer[1024];
		if(GetRawInputData((HRAWINPUT)apLParam, RID_INPUT, vBuffer, &lSize, sizeof(RAWINPUTHEADER)) != lSize) return;

		RAWINPUT *pRaw = (RAWINPUT*)vBuffer;

		//THE line this whole class exists for. Zero means the event was injected
		//with SendInput and has no hardware behind it, which is how a Sunshine or
		//Parsec guest arrives; anything else is a real device on this machine.
		tRawInputDevice pDevice = (tRawInputDevice)pRaw->header.hDevice;

		cRawInputDeviceState &state = mmapDevices[pDevice];
		state.mfLastActivityTime = (double)cPlatform::GetApplicationTime();
		++state.mlEventCount;

		if(pRaw->header.dwType == RIM_TYPEKEYBOARD)
		{
			state.mbIsKeyboard = true;

			const USHORT lVKey = pRaw->data.keyboard.VKey;
			const USHORT lFlags = pRaw->data.keyboard.Flags;

			//255 is the "part of an escaped sequence" filler some keyboards emit.
			if(lVKey == 0xFF) return;

			const bool bExtended = (lFlags & RI_KEY_E0) != 0;
			const bool bDown = (lFlags & RI_KEY_BREAK) == 0;

			eKey key = RawVKeyToKey(lVKey, bExtended);
			if(key == eKey_None || key >= eKey_LastEnum) return;

			state.mvKeyDown[key] = bDown;

			if(bDown) mpLastActiveKeyboard = pDevice;
		}
		else if(pRaw->header.dwType == RIM_TYPEMOUSE)
		{
			state.mbIsMouse = true;

			//Anything at all from a mouse makes it the last active one -- motion or
			//a button. A player being asked to identify their mouse will do either.
			mpLastActiveMouse = pDevice;

			//Relative is what a mouse normally reports and what mouselook wants.
			if((pRaw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
			{
				//A relative device sending 0,0 is a button-only event, not motion.
				if(pRaw->data.mouse.lLastX != 0 || pRaw->data.mouse.lLastY != 0)
				{
					state.mlRelX += pRaw->data.mouse.lLastX;
					state.mlRelY += pRaw->data.mouse.lLastY;
					++state.mlRelEventCount;
				}
			}
			else
			{
				//Absolute: the values are a POSITION, not a movement, so feeding
				//them in as a delta would fling the view across the room on the
				//first event. Differenced against the previous one instead.
				//
				//The first absolute event only establishes the origin -- there is
				//nothing to subtract from yet, and treating it as movement is
				//exactly the fling this avoids.
				const int lX = pRaw->data.mouse.lLastX;
				const int lY = pRaw->data.mouse.lLastY;

				if(state.mbHasLastAbs)
				{
					state.mlRelX += lX - state.mlLastAbsX;
					state.mlRelY += lY - state.mlLastAbsY;
				}

				state.mlLastAbsX = lX;
				state.mlLastAbsY = lY;
				state.mbHasLastAbs = true;
				++state.mlAbsEventCount;
			}

			const USHORT lBtn = pRaw->data.mouse.usButtonFlags;

			if(lBtn & RI_MOUSE_LEFT_BUTTON_DOWN)	state.mvButtonDown[0] = true;
			if(lBtn & RI_MOUSE_LEFT_BUTTON_UP)		state.mvButtonDown[0] = false;
			if(lBtn & RI_MOUSE_MIDDLE_BUTTON_DOWN)	state.mvButtonDown[1] = true;
			if(lBtn & RI_MOUSE_MIDDLE_BUTTON_UP)	state.mvButtonDown[1] = false;
			if(lBtn & RI_MOUSE_RIGHT_BUTTON_DOWN)	state.mvButtonDown[2] = true;
			if(lBtn & RI_MOUSE_RIGHT_BUTTON_UP)		state.mvButtonDown[2] = false;
			if(lBtn & RI_MOUSE_BUTTON_4_DOWN)		state.mvButtonDown[3] = true;
			if(lBtn & RI_MOUSE_BUTTON_4_UP)			state.mvButtonDown[3] = false;
			if(lBtn & RI_MOUSE_BUTTON_5_DOWN)		state.mvButtonDown[4] = true;
			if(lBtn & RI_MOUSE_BUTTON_5_UP)			state.mvButtonDown[4] = false;

			if(lBtn & RI_MOUSE_WHEEL) state.mlWheel += (SHORT)pRaw->data.mouse.usButtonData;
		}
#endif
	}

	//-----------------------------------------------------------------------

	bool cRawInputWin32::KeyIsDownOnDevice(tRawInputDevice aDevice, eKey aKey)
	{
		if(aKey < 0 || aKey >= eKey_LastEnum) return false;

		std::map<tRawInputDevice, cRawInputDeviceState>::iterator it = mmapDevices.find(aDevice);
		if(it == mmapDevices.end()) return false;

		return it->second.mvKeyDown[aKey];
	}

	//-----------------------------------------------------------------------

	bool cRawInputWin32::KeyIsDownExcludingDevice(tRawInputDevice aDevice, eKey aKey)
	{
		if(aKey < 0 || aKey >= eKey_LastEnum) return false;

		//What Player 1 has to be asked, once a second keyboard exists. SDL keeps
		//ONE logical state per key for the whole machine, so with two keyboards it
		//is simply wrong: hold W on one, tap and release it on the other, and SDL
		//says W is up while a finger is still on it.
		std::map<tRawInputDevice, cRawInputDeviceState>::iterator it = mmapDevices.begin();
		for(; it != mmapDevices.end(); ++it)
		{
			if(it->first == aDevice) continue;
			if(it->second.mvKeyDown[aKey]) return true;
		}

		return false;
	}

	//-----------------------------------------------------------------------

	bool cRawInputWin32::ButtonIsDownOnDevice(tRawInputDevice aDevice, int alButton)
	{
		if(alButton < 0 || alButton >= 8) return false;

		std::map<tRawInputDevice, cRawInputDeviceState>::iterator it = mmapDevices.find(aDevice);
		if(it == mmapDevices.end()) return false;

		return it->second.mvButtonDown[alButton];
	}

	//-----------------------------------------------------------------------

	bool cRawInputWin32::ButtonIsDownExcludingDevice(tRawInputDevice aDevice, int alButton)
	{
		if(alButton < 0 || alButton >= 8) return false;

		std::map<tRawInputDevice, cRawInputDeviceState>::iterator it = mmapDevices.begin();
		for(; it != mmapDevices.end(); ++it)
		{
			if(it->first == aDevice) continue;
			if(it->second.mvButtonDown[alButton]) return true;
		}

		return false;
	}

	//-----------------------------------------------------------------------

	void cRawInputWin32::GetRelMotionForDevice(tRawInputDevice aDevice, int *apX, int *apY)
	{
		if(apX) *apX = 0;
		if(apY) *apY = 0;

		std::map<tRawInputDevice, cRawInputDeviceState>::iterator it = mmapDevices.find(aDevice);
		if(it == mmapDevices.end()) return;

		if(apX) *apX = it->second.mlRelX;
		if(apY) *apY = it->second.mlRelY;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cLowLevelInputSDL::cLowLevelInputSDL(iLowLevelGraphics *apLowLevelGraphics)
        : mbLastKeyDownWasPlayer2(false), mpLowLevelGraphics(apLowLevelGraphics), mbQuitMessagePosted(false)
	{
		LockInput(true);
		RelativeMouse(false);
#if SDL_VERSION_ATLEAST(2, 0, 0)
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
#else
//		mlConnectedDevices = 0;
//		mlCheckDeviceChange = 0;
//		mbDirtyGamepads = true;
//
		SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
#endif
	}

	//-----------------------------------------------------------------------

	cLowLevelInputSDL::~cLowLevelInputSDL()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHOD
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void cLowLevelInputSDL::LockInput(bool abX)
	{
		mpLowLevelGraphics->SetWindowGrab(abX);
	}

	void cLowLevelInputSDL::RelativeMouse(bool abX)
	{
		mpLowLevelGraphics->SetRelativeMouse(abX);
	}
	
	//-----------------------------------------------------------------------

	bool cLowLevelInputSDL::EventIsFromPlayer2Keyboard(SDL_Event *apEvent)
	{
		////////////////////////////////////////////////////////////////////////
		// PLAYER 2'S KEYBOARD DOES NOT TYPE INTO PLAYER 1'S CONSOLE.
		//
		// The console and the debug menu are one player's tool on one player's
		// screen, but they are driven by SDL -- and SDL has ONE keyboard for the
		// whole machine. So Player 2 walking around while Player 1 typed spelled
		// wasd into the command line.
		//
		// Only while an overlay is up. Outside one the game does its own
		// per-device filtering further in, and dropping SDL events here would take
		// Player 1's keys with them -- SDL cannot say who pressed what, which is
		// the entire reason this function has to exist.
		if(cImGuiConsole::IsVisible()==false && ImGuiDebugMenu::IsVisible()==false)
		{
			mbLastKeyDownWasPlayer2 = false;
			return false;
		}

		if(ImGuiDebugMenu::GetP2UsesRawInput()==false) return false;
		if(mRawInput.IsAvailable()==false) return false;

		//NO null guard on the handle. kRawInputInjectedDevice IS null -- that is
		//how a streamed guest arrives, because injected input has no hardware
		//behind it and so carries no device. Rejecting null therefore switched
		//this whole filter off in the one setup it was written for, which is why
		//Player 2's keys still landed in the console over Moonlight.
		//
		//Harmless when nothing is injecting: the lookup simply finds no such
		//device, every key reads as not-down on it, and nothing is filtered.
		void *pDevice = ImGuiDebugMenu::GetP2RawDevice();

		switch(apEvent->type)
		{
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			{
				//The SDL event carries no device, but raw input still holds the LEVEL
				//state of every key on every keyboard. A key that is down on Player
				//2's and on nothing else can only have come from Player 2.
				//
				//Level state rather than an edge, deliberately: WM_INPUT and
				//WM_KEYDOWN are two messages for one press and nothing promises their
				//order, but the key stays down on that device for as long as a finger
				//is on it. Same answer however they interleave, and it survives key
				//repeat, which fires the pair over and over.
				//
				//And if BOTH of them are holding it, it goes through: Player 1 losing
				//a keystroke because their partner happened to be walking is worse
				//than a stray character.
				const eKey key = cKeyboardSDL::SDLToKey(apEvent->key.keysym.sym);

				const bool bFromP2 =	mRawInput.KeyIsDownOnDevice(pDevice, key) &&
										mRawInput.KeyIsDownExcludingDevice(pDevice, key)==false;

				if(apEvent->type == SDL_KEYDOWN) mbLastKeyDownWasPlayer2 = bFromP2;

				return bFromP2;
			}

		case SDL_TEXTINPUT:
		case SDL_TEXTEDITING:
			//A character has no key on it at all -- WM_CHAR is a translation of the
			//WM_KEYDOWN in front of it, and Windows delivers them in that order. So
			//it belongs to whoever that key press belonged to, which is the one
			//thing about it that is actually known.
			return mbLastKeyDownWasPlayer2;

		default:
			break;
		}

		return false;
	}

	//-----------------------------------------------------------------------

	void cLowLevelInputSDL::BeginInputUpdate()
	{
		SDL_Event sdlEvent;

		//Started here rather than in the constructor because it hooks SDL's message
		//pump, which wants the window to exist first. Init() is a no-op once it has
		//taken, so this costs one bool test a frame.
		mRawInput.Init();
		mRawInput.BeginFrame();

		mlstEvents.clear();
		while(SDL_PollEvent(&sdlEvent)!=0)
		{
			//Dropped before ImGui ever sees it. Nothing further down wants it
			//either: the overlay swallows keyboard and text events from the game
			//list anyway, so this is the whole journey for one of Player 2's keys.
			if(EventIsFromPlayer2Keyboard(&sdlEvent)) continue;

			ImGuiManager::ProcessEvent(&sdlEvent);

			//////////////////////////////////////////////////////////////////////
			// Window and display state, logged as it happens -- TEMPORARY
			//
			// The per-frame brightness heartbeat proves the engine hands a fully lit
			// frame to the driver on every single frame, including throughout the
			// stretches where the screen is black. Nothing is wrong with the picture
			// being produced, so it is being lost after the swap -- and the only part
			// of that this process can still observe is what the OS tells it.
			//
			// A focus change, a size change, a monitor changing mode or reconnecting,
			// and a GPU context reset all arrive here. A burst of these sitting at the
			// timestamp where the screen went black names the cause; no burst there
			// rules the whole category out and points at the cable or the panel.
			//
			// Free: none of these fire during a normal frame.
#if SDL_VERSION_ATLEAST(2, 0, 0)
			if(sdlEvent.type == SDL_WINDOWEVENT)
			{
				const int lEvent = sdlEvent.window.event;

				//Mouse crossing the window edge and plain moves are noise.
				if(	lEvent != SDL_WINDOWEVENT_ENTER &&
					lEvent != SDL_WINDOWEVENT_LEAVE &&
					lEvent != SDL_WINDOWEVENT_MOVED)
				{
					SDL_Window *pWin = SDL_GL_GetCurrentWindow();
					int lW = 0, lH = 0, lDisplay = -1;
					unsigned int lFlags = 0;
					if(pWin)
					{
						SDL_GetWindowSize(pWin, &lW, &lH);
						lDisplay = SDL_GetWindowDisplayIndex(pWin);
						lFlags = SDL_GetWindowFlags(pWin);
					}

					LogVerbose("[window] %lu ms: event=%d data=(%d,%d) size=%dx%d display=%d flags=0x%X\n",
						cPlatform::GetApplicationTime(), lEvent,
						(int)sdlEvent.window.data1, (int)sdlEvent.window.data2,
						lW, lH, lDisplay, lFlags);
				}
			}
			else if(	sdlEvent.type == SDL_RENDER_DEVICE_RESET ||
						sdlEvent.type == SDL_RENDER_TARGETS_RESET)
			{
				LogVerbose("[window] %lu ms: GPU CONTEXT RESET (sdl type %d) -- the driver restarted\n",
					cPlatform::GetApplicationTime(), (int)sdlEvent.type);
			}
#if SDL_VERSION_ATLEAST(2, 0, 9)
			else if(sdlEvent.type == SDL_DISPLAYEVENT)
			{
				LogVerbose("[window] %lu ms: DISPLAY event=%d display=%d data=%d"
					" -- a monitor changed mode, orientation or connection\n",
					cPlatform::GetApplicationTime(), (int)sdlEvent.display.event,
					(int)sdlEvent.display.display, (int)sdlEvent.display.data1);
			}
#endif
#endif
#if defined _WIN32 && !SDL_VERSION_ATLEAST(2,0,0)
			if(sdlEvent.type==SDL_SYSWMEVENT)
			{
				SDL_SysWMmsg* pMsg = sdlEvent.syswm.msg;
				
				// This is bad, cos it is actually Windows specific code, should not be here. TODO: move it, obviously
				if(pMsg->msg==WM_DEVICECHANGE)
				{
					if(pMsg->wParam==DBT_DEVICEARRIVAL)
					{
						cEngine::SetDeviceWasPlugged();
					}
					else if(pMsg->wParam==DBT_DEVICEREMOVECOMPLETE)
					{
						cEngine::SetDeviceWasRemoved();
					}
				}
			}
			else
#endif //WIN32
#if SDL_VERSION_ATLEAST(2, 0, 0)
            // built-in SDL2 gamepad hotplug code
            // this whole contract should be rewritten to allow clean adding/removing
            // of controllers, instead of brute force rescanning
            if (sdlEvent.type==SDL_CONTROLLERDEVICEADDED)
            {
                // sdlEvent.cdevice.which is the device #
                cEngine::SetDeviceWasPlugged();
            } else if (sdlEvent.type==SDL_CONTROLLERDEVICEREMOVED)
            {
                // sdlEvent.cdevice.which is the instance # (not device #).
                // instance # increases as devices are plugged and unplugged.
                cEngine::SetDeviceWasRemoved();
            }
#endif
#if defined (__APPLE__)
            if (sdlEvent.type==SDL_KEYDOWN)
            {
                if (sdlEvent.key.keysym.sym == SDLK_q && sdlEvent.key.keysym.mod & KMOD_GUI) {
                    mbQuitMessagePosted = true;
                } else {
                    mlstEvents.push_back(sdlEvent);
                }
            } else
#endif
            if (sdlEvent.type==SDL_QUIT)
            {
                mbQuitMessagePosted = true;
            } else
			{
				////////////////////////////////////////////////////////////////
				// With the console or the debug menu open, the game must not see
				// input at all.
				//
				// ImGuiManager::ProcessEvent already swallows the events it acts
				// on, but everything else still landed in this list, so typing
				// "noclip" also walked the player forwards, crouched, and lit the
				// lantern. Dropping the keyboard, text, mouse and wheel events
				// here is what actually stops that -- the overlay is modal or it
				// is not.
				//
				// Window, quit and controller hot-plug events are deliberately
				// still let through: none of them are the player doing something,
				// and the engine needs all of them regardless of what is on top.
				bool bSwallow = false;

				if (cImGuiConsole::IsVisible() || ImGuiDebugMenu::IsVisible())
				{
					switch (sdlEvent.type)
					{
					case SDL_KEYDOWN:
					case SDL_KEYUP:
					case SDL_TEXTINPUT:
					case SDL_TEXTEDITING:
					case SDL_MOUSEMOTION:
					case SDL_MOUSEBUTTONDOWN:
					case SDL_MOUSEBUTTONUP:
					case SDL_MOUSEWHEEL:
						bSwallow = true;
						break;
					default:
						break;
					}
				}

				if (bSwallow == false)
					mlstEvents.push_back(sdlEvent);
			}
		}
	}
	
	//-----------------------------------------------------------------------

	void cLowLevelInputSDL::EndInputUpdate()
	{
		
	}

	//-----------------------------------------------------------------------

	void cLowLevelInputSDL::InitGamepadSupport()
	{
#if !SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_InitSubSystem(SDL_INIT_JOYSTICK);
#endif
	}

	void cLowLevelInputSDL::DropGamepadSupport()
	{
#if !SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
#endif
	}

	int cLowLevelInputSDL::GetPluggedGamepadNum()
	{
#if USE_XINPUT
		return cGamepadXInput::GetNumConnected();
#else
		return SDL_NumJoysticks();
#endif
	}

	//-----------------------------------------------------------------------
	
	iMouse* cLowLevelInputSDL::CreateMouse()
	{
		return hplNew( cMouseSDL,(this));
	}
	
	//-----------------------------------------------------------------------
	
	iKeyboard* cLowLevelInputSDL::CreateKeyboard()
	{
		return hplNew( cKeyboardSDL,(this) );
	}

	//-----------------------------------------------------------------------

	iGamepad* cLowLevelInputSDL::CreateGamepad(int alIndex)
	{
#if USE_SDL2
		return hplNew( cGamepadSDL2, (this, alIndex) );
#else
		return hplNew( cGamepadSDL, (this, alIndex) );
#endif
	}
	
	//-----------------------------------------------------------------------

    bool cLowLevelInputSDL::isQuitMessagePosted()
    {
        return mbQuitMessagePosted;
    }
    
    void cLowLevelInputSDL::resetQuitMessagePosted()
    {
        mbQuitMessagePosted = false;
    }

	//-----------------------------------------------------------------------
    
}
