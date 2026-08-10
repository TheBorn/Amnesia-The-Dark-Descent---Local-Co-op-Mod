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

#include "impl/MouseSDL.h"

#if USE_SDL2
#include "SDL2/SDL.h"
#else
#include "SDL/SDL.h"
#endif

#include "graphics/LowLevelGraphics.h"
#include "impl/LowLevelInputSDL.h"
#include "impl/ImGuiConsole.h"
#include "impl/ImGuiDebugMenu.h"
#include "math/Math.h"

#include "system/LowLevelSystem.h"
#include "system/Platform.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cMouseSDL::cMouseSDL(cLowLevelInputSDL *apLowLevelInputSDL) : iMouse("SDL Portable Mouse")
	{
		mvMButtonArray.resize(eMouseButton_LastEnum);
		mvMButtonArray.assign(mvMButtonArray.size(),false);

		mpLowLevelInputSDL = apLowLevelInputSDL;

		mvMouseRelPos = cVector2l(0,0);
		mvMouseAbsPos = cVector2l(0,0);

		mbWheelUpMoved = false;
		mbWheelDownMoved = false;

		mbFirstTime = true;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void cMouseSDL::Update()
	{
		///////////////////////////////////////
		//On first update make sure to clear all previous events.
		if(mbFirstTime)
		{
			//Clear all relative movement
			for(int i=0; i<10; ++i)
			{
				SDL_PumpEvents();
				int lX,lY;
				SDL_GetRelativeMouseState(&lX, &lY);
			}
			mbFirstTime = false;
		}
		
		iLowLevelGraphics *pLowLevelGfx = mpLowLevelInputSDL->GetLowLevelGraphics();
		//mvMouseRelPos = cVector2f(0,0);
		mbWheelUpMoved = false;
		mbWheelDownMoved = false;

		std::list<SDL_Event>::iterator it = mpLowLevelInputSDL->mlstEvents.begin();
		for(; it != mpLowLevelInputSDL->mlstEvents.end(); ++it)
		{
			SDL_Event *pEvent = &(*it);

			if(	pEvent->type != SDL_MOUSEMOTION && 
				pEvent->type != SDL_MOUSEBUTTONDOWN &&
#if SDL_VERSION_ATLEAST(2, 0, 0)
                pEvent->type != SDL_MOUSEWHEEL &&
#endif
				pEvent->type != SDL_MOUSEBUTTONUP)
			{
				continue;
			}

			if(pEvent->type == SDL_MOUSEMOTION)
			{
#if SDL_VERSION_ATLEAST(2, 0, 0) && _WIN32
				/*if(pLowLevelGfx->GetFullscreenModeActive() == false)
				{
					/////////////
					// Only use abs position if not in fullscreen mode
					mvMouseAbsPos = cVector2l(pEvent->motion.x,pEvent->motion.y);
				}*/
				mvMouseAbsPos = cVector2l(pEvent->motion.x,pEvent->motion.y);
#else
				mvMouseAbsPos = cVector2l(pEvent->motion.x,pEvent->motion.y);
#endif
				
				Uint8 buttonState = pEvent->motion.state;

				//Set button here as well just to be sure
				/*if(buttonState & SDL_BUTTON(1)) mvMButtonArray[eMouseButton_Left] = true;
				if(buttonState & SDL_BUTTON(2)) mvMButtonArray[eMouseButton_Middle] = true;
				if(buttonState & SDL_BUTTON(3)) mvMButtonArray[eMouseButton_Right] = true;*/
			}
#if SDL_VERSION_ATLEAST(2, 0, 0)
            else if(pEvent->type == SDL_MOUSEWHEEL)
            {
                if (pEvent->wheel.y > 0) {
                    mvMButtonArray[eMouseButton_WheelUp] = true;
                    mbWheelUpMoved = true;
                } else {
                    mvMButtonArray[eMouseButton_WheelDown] = true;
                    mbWheelDownMoved = true;
                }
                break;
            }
#endif
			else
			{
				bool bButtonIsDown = pEvent->type==SDL_MOUSEBUTTONDOWN;

				//if(pEvent->button.button == SDL_BUTTON_WHEELUP)Log(" Wheel %d!\n",bButtonIsDown);

				switch(pEvent->button.button)
				{
					case SDL_BUTTON_LEFT: mvMButtonArray[eMouseButton_Left] = bButtonIsDown;break;
					case SDL_BUTTON_MIDDLE: mvMButtonArray[eMouseButton_Middle] = bButtonIsDown;break;
					case SDL_BUTTON_RIGHT: mvMButtonArray[eMouseButton_Right] = bButtonIsDown;break;
					case SDL_BUTTON_X1: mvMButtonArray[eMouseButton_Button6] = bButtonIsDown;break;
					case SDL_BUTTON_X2: mvMButtonArray[eMouseButton_Button7] = bButtonIsDown;break;
#if !SDL_VERSION_ATLEAST(2, 0, 0)
					case SDL_BUTTON_WHEELUP: 
						mvMButtonArray[eMouseButton_WheelUp] = bButtonIsDown;
						if(bButtonIsDown) mbWheelUpMoved = true;
						break;
					case SDL_BUTTON_WHEELDOWN: 
						mvMButtonArray[eMouseButton_WheelDown] = bButtonIsDown;
						if(bButtonIsDown) mbWheelDownMoved = true;
						break;
#endif
				}
			}
		}

		if(mbWheelDownMoved)	mvMButtonArray[eMouseButton_WheelDown] = true;
		else					mvMButtonArray[eMouseButton_WheelDown] = false;
		if(mbWheelUpMoved)		mvMButtonArray[eMouseButton_WheelUp] = true;
		else					mvMButtonArray[eMouseButton_WheelUp] = false;
		
		int lX,lY; 
		SDL_GetRelativeMouseState(&lX, &lY);

		//////////////////////////////////////////////////////////////////////
		// Taken and thrown away while the console or the debug menu is up.
		//
		// cLowLevelInputSDL already drops mouse EVENTS for the overlay -- the
		// overlay is modal or it is not. This line is not an event read: it polls
		// SDL directly, so the motion arrived anyway and the player carried on
		// looking around while typing into the console.
		//
		// In co-op it is worse than untidy. The overlay turns relative mouse mode
		// off, and SDL unregisters its raw mouse input along with it -- so Player
		// 2's share of this total can no longer be measured, and the subtraction
		// that normally keeps their mouse out of Player 1's head has nothing to
		// subtract. Player 2 aiming turned Player 1 around, for exactly as long as
		// the debug menu was open.
		//
		// Still READ, never skipped: SDL accumulates relative motion whether or
		// not anybody asks for it, so leaving it would hand the whole stack of it
		// to Player 1 in one jump the moment the overlay closed.
		if(cImGuiConsole::IsVisible() || ImGuiDebugMenu::IsVisible())
		{
			lX = 0;
			lY = 0;
		}

		mvMouseRelPos = cVector2l(lX,lY);

		
	}
	
	//-----------------------------------------------------------------------
	
	bool cMouseSDL::ButtonIsDown(eMouseButton mButton)
	{
		return mvMButtonArray[mButton];
	}

	//-----------------------------------------------------------------------

	cVector2l cMouseSDL::GetAbsPosition()
	{
		return mvMouseAbsPos;
	}
	
	//-----------------------------------------------------------------------

	cVector2l cMouseSDL::GetRelPosition()
	{
		cVector2l vPos = mvMouseRelPos;
		mvMouseRelPos = cVector2l(0,0);
		
		return vPos;
	}

	//-----------------------------------------------------------------------
	
	/////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	/////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	//-----------------------------------------------------------------------

}
