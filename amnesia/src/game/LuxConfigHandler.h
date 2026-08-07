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

#ifndef LUX_CONFIG_HANDLER_H
#define LUX_CONFIG_HANDLER_H

//----------------------------------------------

#include "LuxBase.h"

//----------------------------------------------

class cLuxConfigHandler
{
public:	
	cLuxConfigHandler();
	~cLuxConfigHandler();

	void LoadMainConfig();
	void SaveMainConfig();

	/**
	 * Push the Debug and Coop settings loaded above into the places that
	 * actually read them -- ImGuiManager's hotkey gates, the verbose log switch,
	 * ImGuiDebugMenu's option statics and the map handler's split-screen mode.
	 *
	 * Separate from LoadMainConfig because that runs before the engine and the
	 * game modules exist. Called once at the end of startup, and again by the
	 * options menu every time the player presses OK.
	 */
	void ApplyDebugAndCoopSettings();

	bool ShowRestartWarning(cGuiSet* apSet, void* apObject, tGuiCallbackFunc apCallback);

	void SetGameNeedsRestart() { mbGameNeedsRestart=true; mbRestartDialogShown=false; }

	// Variables
	bool mbLoadDebugMenu;
	bool mbFirstStart;
	tString msScreenShotExt;

	bool mbCreateAndLoadCompressedMaps;
	bool mbForceCacheLoadingAndSkipSaving;

	tString msLangFile;
	
	cVector2l mvScreenSize;
    int mlDisplay;
	bool mbFullscreen;
	bool mbVSync;
	bool mbAdaptiveVSync;
	int mlTextureQuality;
	int mlTextureFilter;
	float mfTextureAnisotropy;
	int mlShadowQuality;
	int mlShadowRes;

	bool mbSSAOActive;
	int mlSSAOSamples;
	int mlSSAOResolution; //0= medium(div2), 1=high (same as screen resolution)
	
	int mlParallaxQuality;
	bool mbParallaxEnabled;

	bool mbOcclusionTestLights;
	
	bool mbEdgeSmooth;
		
	bool mbWorldReflection;
	bool mbRefraction;
	bool mbShadowsActive;

	bool mbForceShaderModel3And4Off;

	bool mbFastPhysicsLoad;
	bool mbFastStaticLoad;
	bool mbFastEntityLoad;

	int mlSoundDevID;
	int mlMaxSoundChannels;
	int mlSoundStreamBuffers;
	int mlSoundStreamBufferSize;

	//////////////////////////////////////////////////////////////////
	// Debug features -- Options -> Debug.
	//
	// All off by default: the console, the ImGui debug menu and the verbose
	// render logging are developer tools, and their hotkeys do nothing until a
	// player deliberately switches them on. mbDebugWarningAccepted records that
	// the "use at your own risk" box has been shown once and dismissed.
	bool mbDebugConsoleEnabled;
	bool mbDebugMenuEnabled;
	bool mbVerboseLogging;
	bool mbDebugWarningAccepted;

	//////////////////////////////////////////////////////////////////
	// Co-op -- Options -> Coop.
	//
	// mlCoopSplitMode matches cLuxMapHandler::eSplitScreenMode (1 = left/right,
	// 2 = top/bottom, 3 = dual monitor); mlCoopDualMonitorIndex is the SDL
	// display index used when the mode is 3, or -1 for none.
	int  mlCoopSplitMode;
	int  mlCoopDualMonitorIndex;

	// Experimental rigged player models. The two indices are remembered even
	// while the gate is off, so turning it back on restores the choice.
	bool mbCoopAllowModelChange;
	bool mbCoopModelWarningAccepted;
	int  mlCoopModelP1;
	int  mlCoopModelP2;

	// Forced-coop compatibility. Named as the options menu states them, which
	// for two of them is the inverse of how the engine stores it:
	// mbCoopBothAtLevelDoor is !LevelDoorSinglePlayer and mbCoopSharedLantern is
	// !LanternPerPlayer. ApplyDebugAndCoopSettings does the flip.
	bool mbCoopScriptTeleportBoth;
	bool mbCoopSharedFlashbacks;
	bool mbCoopBothAtLevelDoor;
	bool mbCoopSharedSanityReward;
	bool mbCoopSharedLantern;
	bool mbCoopShareOilEffect;
	bool mbCoopShareLaudanumEffect;
	bool mbCoopShareSanityPotionEffect;
	bool mbCoopShareTinderboxes;
	bool mbCoopP2Callbacks;
	bool mbCoopFullItemShare;

	// The forced-coop "things were not meant to be this way" box, shown once
	// before the main story is started in co-op.
	bool mbCoopForcedWarningAccepted;

	
private:
	bool mbGameNeedsRestart;
	bool mbRestartDialogShown;
};

//----------------------------------------------


#endif // LUX_DEBUG_HANDLER_H
