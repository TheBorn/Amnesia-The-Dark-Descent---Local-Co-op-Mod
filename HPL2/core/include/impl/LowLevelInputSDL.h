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

#ifndef HPL_LOWLEVELINPUT_SDL_H
#define HPL_LOWLEVELINPUT_SDL_H

#include <list>
#include <map>
#include <vector>
#include "input/LowLevelInput.h"
#include "input/InputTypes.h"

#if USE_SDL2
#include "SDL2/SDL_events.h"
#else
#include "SDL/SDL_events.h"
#endif

namespace hpl {

	class iLowLevelGraphics;

	//----------------------------------------------------------------------
	// SECOND KEYBOARD AND MOUSE, via Windows Raw Input.
	//
	// SDL2 has exactly one system keyboard and one system cursor -- there is no
	// per-device concept anywhere in its API, so two keyboards are one keyboard as
	// far as it is concerned. Windows does not have that limitation: every WM_INPUT
	// message carries a RAWINPUTHEADER.hDevice handle identifying the physical
	// device it came from, which is the whole reason this exists.
	//
	// This runs ALONGSIDE SDL rather than replacing it. Raw input is an extra
	// stream, not a substitute: SDL still delivers every one of these events
	// through its own path with no idea which device sent it. Undoing that is the
	// game layer's job -- see cLuxInputHandler.
	//
	// WINDOWS ONLY. Everything below compiles to nothing elsewhere, and every
	// caller has to cope with IsAvailable() being false.
	//----------------------------------------------------------------------

	//A device handle. Kept as a void* rather than a HANDLE so nothing outside the
	//implementation has to include windows.h.
	typedef void* tRawInputDevice;

	//What Sunshine, Parsec, AutoHotkey and anything else driving SendInput shows up
	//as. Injected input still reaches raw input, but with no device behind it, so
	//hDevice is zero -- which is exactly what makes a streamed guest separable from
	//the person sitting at the machine, with no pairing step at all.
	#define kRawInputInjectedDevice ((hpl::tRawInputDevice)0)

	class cRawInputDeviceState
	{
	public:
		cRawInputDeviceState();

		//Live key state, indexed by eKey. Driven by this device alone.
		std::vector<bool> mvKeyDown;

		//Accumulated since the last frame, consumed by whoever reads it.
		int mlRelX;
		int mlRelY;
		int mlWheel;

		bool mvButtonDown[8];

		//For the device list in the debug menu: last time anything arrived, and a
		//running count, so a player can tell which keyboard is which by typing on it.
		double mfLastActivityTime;
		int mlEventCount;
		bool mbIsKeyboard;
		bool mbIsMouse;

		//Absolute reporters exist and have to be handled rather than dropped: a
		//streaming host injecting through SendInput sends screen coordinates unless
		//the client is in relative/capture mode, and so do tablets and some remote
		//desktops. Their motion is turned into a delta against the previous
		//position instead. Counted separately so the debug list can say which kind
		//a device is actually sending, because the two need opposite handling and
		//guessing wrong means either no mouselook at all or the view flung across
		//the room on the first event.
		int mlRelEventCount;
		int mlAbsEventCount;
		bool mbHasLastAbs;
		int mlLastAbsX;
		int mlLastAbsY;
	};

	class cRawInputWin32
	{
	public:
		cRawInputWin32();
		~cRawInputWin32();

		/** Hook the message stream and register for raw keyboard. Safe to call twice. */
		void Init();

		/** True once Init has succeeded. Always false off Windows. */
		bool IsAvailable(){ return mbAvailable;}

		/** Clear the per-frame deltas. Key state is level and survives. */
		void BeginFrame();

		/** Every device seen so far, newest activity last. */
		const std::map<tRawInputDevice, cRawInputDeviceState>& GetDevices(){ return mmapDevices;}

		/** Key state for one device, or for every device EXCEPT one. */
		bool KeyIsDownOnDevice(tRawInputDevice aDevice, eKey aKey);
		bool KeyIsDownExcludingDevice(tRawInputDevice aDevice, eKey aKey);

		bool ButtonIsDownOnDevice(tRawInputDevice aDevice, int alButton);
		bool ButtonIsDownExcludingDevice(tRawInputDevice aDevice, int alButton);

		/** Movement this frame from one device, in raw counts. */
		void GetRelMotionForDevice(tRawInputDevice aDevice, int *apX, int *apY);

		/** The device that most recently sent a key down, for a press-to-assign step. */
		tRawInputDevice GetLastActiveKeyboard(){ return mpLastActiveKeyboard;}

		/**
		 * The device that most recently MOVED, for the same press-to-assign step.
		 *
		 * A keyboard and a mouse are two separate raw devices with two separate
		 * handles, so knowing which keyboard is Player 2's says nothing at all
		 * about which mouse is. Every mouse question asked of a keyboard handle
		 * answers "no motion, no buttons" -- which is silent, and looks exactly
		 * like the separation simply not working.
		 */
		tRawInputDevice GetLastActiveMouse(){ return mpLastActiveMouse;}

		/** Called from the Windows message hook. Public only because that hook is static. */
		void HandleRawInputMessage(void *apLParam);

		/**
		 * The one live instance, or NULL before the input system is up.
		 *
		 * Saves every caller from casting cInput::GetLowLevel() down to the SDL
		 * implementation just to reach a Windows-only extra, and there is only ever
		 * one of these -- cLowLevelInputSDL owns it by value.
		 */
		static cRawInputWin32* GetInstance(){ return mpInstance;}

	private:
		static cRawInputWin32 *mpInstance;
		bool mbAvailable;
		std::map<tRawInputDevice, cRawInputDeviceState> mmapDevices;
		tRawInputDevice mpLastActiveKeyboard;
		tRawInputDevice mpLastActiveMouse;
	};

	class cLowLevelInputSDL : public iLowLevelInput
	{
	public:
		cLowLevelInputSDL(iLowLevelGraphics *apLowLevelGraphics);
		~cLowLevelInputSDL();

		void LockInput(bool abX);
		void RelativeMouse(bool abX);

		void BeginInputUpdate();
		void EndInputUpdate();

		void InitGamepadSupport();
		void DropGamepadSupport();

		int GetPluggedGamepadNum();

		iMouse* CreateMouse();
		iKeyboard* CreateKeyboard();
		iGamepad* CreateGamepad(int alIndex);

		iLowLevelGraphics* GetLowLevelGraphics() { return mpLowLevelGraphics; }

		bool isQuitMessagePosted();
		void resetQuitMessagePosted();

		/** The second-device layer. Never NULL; ask it IsAvailable(). */
		cRawInputWin32* GetRawInput(){ return &mRawInput;}

	public:
		std::list<SDL_Event> mlstEvents;

	private: 
		/**
		 * Did this SDL keyboard or text event come from PLAYER 2's keyboard, while
		 * an overlay that belongs to Player 1 is up?
		 */
		bool EventIsFromPlayer2Keyboard(SDL_Event *apEvent);

		//A character event carries no key, so it inherits the verdict of the key
		//press it was translated from -- the one immediately before it.
		bool mbLastKeyDownWasPlayer2;

		iLowLevelGraphics *mpLowLevelGraphics;
		bool mbQuitMessagePosted;
		cRawInputWin32 mRawInput;
	};
};
#endif // HPL_LOWLEVELINPUT_SDL_H
