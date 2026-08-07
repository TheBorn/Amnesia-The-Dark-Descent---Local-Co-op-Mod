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

#include "LuxConfigHandler.h"

#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxInputHandler.h"
#include "LuxHintHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxPlayer.h"

#include "impl/ImGuiManager.h"
#include "impl/ImGuiDebugMenu.h"

// TODO: there is some cleanup to do here

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxConfigHandler::cLuxConfigHandler()
{
	mbGameNeedsRestart = false;
	mbRestartDialogShown = false;
}

//-----------------------------------------------------------------------

cLuxConfigHandler::~cLuxConfigHandler()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------

void cLuxConfigHandler::LoadMainConfig()
{
	////////////////////////////////////////////////////////////////////////////////
	// Purpose of this is loading variables that need to be set on engine creation,
	// or set to restart.

	/////////////////////
	// Main
	mbLoadDebugMenu = gpBase->mpMainConfig->GetBool("Main","LoadDebugMenu", false);
	mbFirstStart = gpBase->CheckFirstStartFlag()==false;
	//mbFirstStart = gpBase->mpMainConfig->GetBool("Main","FirstStart", true);

	msLangFile = gpBase->mpMainConfig->GetString("Main", "StartLanguage", gpBase->msDefaultGameLanguage);

	msScreenShotExt = gpBase->mpMainConfig->GetString("Main","ScreenShotExt", "jpg");

	mbForceCacheLoadingAndSkipSaving = gpBase->mpMainConfig->GetBool("Main","ForceCacheLoadingAndSkipSaving", true);
	//mbCreateAndLoadCompressedMaps = gpBase->mpMainConfig->GetBool("Main","CreateAndLoadCompressedMaps", false);

	/////////////////////
	// Engine init variables
	mvScreenSize.x =	gpBase->mpMainConfig->GetInt("Screen","Width", 800);
	mvScreenSize.y =	gpBase->mpMainConfig->GetInt("Screen","Height", 600);
    mlDisplay =			gpBase->mpMainConfig->GetInt("Screen","Display", 0);
	mbFullscreen =		gpBase->mpMainConfig->GetBool("Screen","FullScreen", false);
	mbVSync =			gpBase->mpMainConfig->GetBool("Screen","Vsync", false);
	mbAdaptiveVSync =	gpBase->mpMainConfig->GetBool("Screen","AdaptiveVsync", false);

	mbFastPhysicsLoad=	gpBase->mpMainConfig->GetBool("MapLoad","FastPhysicsLoad", false);
	mbFastStaticLoad=	gpBase->mpMainConfig->GetBool("MapLoad","FastStaticLoad", false);
	mbFastEntityLoad =	gpBase->mpMainConfig->GetBool("MapLoad","FastEntityLoad", false);

	/////////////////////
	// Graphics variables

	// General
	mbOcclusionTestLights = gpBase->mpMainConfig->GetBool("Graphics", "OcclusionTestLights", true);

	// Shadows
	mbShadowsActive =	gpBase->mpMainConfig->GetBool("Graphics", "ShadowsActive", true);
	mlShadowQuality =	gpBase->mpMainConfig->GetInt("Graphics", "ShadowQuality", eShadowMapQuality_Medium);
	mlShadowRes =		gpBase->mpMainConfig->GetInt("Graphics", "ShadowResolution", eShadowMapResolution_High);
	
	// Misc
	mbWorldReflection = gpBase->mpMainConfig->GetBool("Graphics", "WorldReflection", true);
	mbRefraction =		gpBase->mpMainConfig->GetBool("Graphics", "Refraction", true);
	mbEdgeSmooth =		gpBase->mpMainConfig->GetBool("Graphics", "EdgeSmooth", false);

	// SSAO
	mbSSAOActive =		gpBase->mpMainConfig->GetBool("Graphics","SSAOActive", true);
	mlSSAOSamples =		gpBase->mpMainConfig->GetInt("Graphics","SSAOSamples", 8);
	mlSSAOResolution =	gpBase->mpMainConfig->GetInt("Graphics","SSAOResolution", 0);
	
	// Parallax
	mbParallaxEnabled = gpBase->mpMainConfig->GetBool("Graphics", "ParallaxEnabled", true);
	mlParallaxQuality = gpBase->mpMainConfig->GetInt("Graphics", "ParallaxQuality", 0);
	
	// Texture
	mlTextureQuality =	gpBase->mpMainConfig->GetInt("Graphics", "TextureQuality", 0);
	mlTextureFilter =	gpBase->mpMainConfig->GetInt("Graphics", "TextureFilter", eTextureFilter_Bilinear);
	mfTextureAnisotropy = gpBase->mpMainConfig->GetFloat("Graphics", "TextureAnisotropy", 1.0f);

	mbForceShaderModel3And4Off = gpBase->mpMainConfig->GetBool("Graphics", "ForceShaderModel3And4Off", false);

	/////////////////////
	// Sound variables
	mlSoundDevID = gpBase->mpMainConfig->GetInt("Sound", "Device", -1);
	mlMaxSoundChannels = gpBase->mpMainConfig->GetInt("Sound", "MaxChannels", 32);
	mlSoundStreamBuffers = gpBase->mpMainConfig->GetInt("Sound", "StreamBuffers", 4);
	mlSoundStreamBufferSize = gpBase->mpMainConfig->GetInt("Sound", "StreamBufferSize", 262144);

	/////////////////////
	// Debug features
	mbDebugConsoleEnabled  = gpBase->mpMainConfig->GetBool("Debug","ConsoleEnabled", false);
	mbDebugMenuEnabled     = gpBase->mpMainConfig->GetBool("Debug","DebugMenuEnabled", false);
	mbVerboseLogging       = gpBase->mpMainConfig->GetBool("Debug","VerboseLogging", false);
	mbDebugWarningAccepted = gpBase->mpMainConfig->GetBool("Debug","WarningAccepted", false);

	/////////////////////
	// Co-op
	//
	// Every default below is the value the option already had as a hard-coded
	// static, so a config file without a Coop section behaves exactly as the
	// build did before these settings existed.
	mlCoopSplitMode        = gpBase->mpMainConfig->GetInt("Coop","SplitMode", 1);
	mlCoopDualMonitorIndex = gpBase->mpMainConfig->GetInt("Coop","DualMonitorIndex", -1);

	mbCoopAllowModelChange     = gpBase->mpMainConfig->GetBool("Coop","AllowModelChange", false);
	mbCoopModelWarningAccepted = gpBase->mpMainConfig->GetBool("Coop","ModelWarningAccepted", false);
	mlCoopModelP1              = gpBase->mpMainConfig->GetInt("Coop","ModelP1", 0);
	mlCoopModelP2              = gpBase->mpMainConfig->GetInt("Coop","ModelP2", 0);

	mbCoopScriptTeleportBoth      = gpBase->mpMainConfig->GetBool("Coop","TeleportBothPlayers", true);
	mbCoopSharedFlashbacks        = gpBase->mpMainConfig->GetBool("Coop","SharedFlashbacks", true);
	mbCoopBothAtLevelDoor         = gpBase->mpMainConfig->GetBool("Coop","BothAtLevelDoor", false);
	mbCoopSharedSanityReward      = gpBase->mpMainConfig->GetBool("Coop","SharedSanityReward", true);
	mbCoopSharedLantern           = gpBase->mpMainConfig->GetBool("Coop","SharedLantern", false);
	mbCoopShareOilEffect          = gpBase->mpMainConfig->GetBool("Coop","SharedOilEffect", true);
	mbCoopShareLaudanumEffect     = gpBase->mpMainConfig->GetBool("Coop","SharedLaudanumEffect", true);
	mbCoopShareSanityPotionEffect = gpBase->mpMainConfig->GetBool("Coop","SharedSanityPotionEffect", true);
	mbCoopShareTinderboxes        = gpBase->mpMainConfig->GetBool("Coop","SharedTinderboxes", false);
	mbCoopP2Callbacks             = gpBase->mpMainConfig->GetBool("Coop","Player2Callbacks", true);
	mbCoopFullItemShare           = gpBase->mpMainConfig->GetBool("Coop","FullItemShare", false);

	mbCoopForcedWarningAccepted = gpBase->mpMainConfig->GetBool("Coop","ForcedWarningAccepted", false);
}

//-----------------------------------------------------------------------

void cLuxConfigHandler::SaveMainConfig()
{
	/////////////////////
	// Main
	gpBase->mpMainConfig->SetBool("Main","LoadDebugMenu", mbLoadDebugMenu);
	if(mbFirstStart) gpBase->RaiseFirstStartFlag();
	//gpBase->mpMainConfig->SetBool("Main","FirstStart", false);


	gpBase->mpMainConfig->SetString("Main", "StartLanguage", msLangFile);

	gpBase->mpMainConfig->SetString("Main","ScreenShotExt", msScreenShotExt);

	gpBase->mpMainConfig->SetBool("Main","ForceCacheLoadingAndSkipSaving", mbForceCacheLoadingAndSkipSaving);

	/////////////////////
	// Engine init variables
	gpBase->mpMainConfig->SetInt("Screen","Width", mvScreenSize.x);
	gpBase->mpMainConfig->SetInt("Screen","Height", mvScreenSize.y);
	gpBase->mpMainConfig->SetBool("Screen","FullScreen", mbFullscreen);
	gpBase->mpMainConfig->SetBool("Screen","Vsync", mbVSync);

	gpBase->mpMainConfig->SetBool("MapLoad","FastPhysicsLoad", mbFastPhysicsLoad);
	gpBase->mpMainConfig->SetBool("MapLoad","FastStaticLoad", mbFastStaticLoad);
	gpBase->mpMainConfig->SetBool("MapLoad","FastEntityLoad", mbFastEntityLoad);

	/////////////////////
	// Graphics variables
	cMaterialManager* pMatMgr = gpBase->mpEngine->GetResources()->GetMaterialManager();
	cRenderSettings* pRenderSettings = gpBase->mpMapHandler->GetViewport()->GetRenderSettings();

	gpBase->mpMainConfig->SetFloat("Graphics","Gamma",gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetGammaCorrection());
	gpBase->mpMainConfig->SetInt("Graphics","GBufferType", cRendererDeferred::GetGBufferType());
	gpBase->mpMainConfig->SetInt("Graphics","NumOfGBufferTextures", cRendererDeferred::GetNumOfGBufferTextures());
	
	gpBase->mpMainConfig->SetBool("Graphics", "OcclusionTestLights", mbOcclusionTestLights);

	gpBase->mpMainConfig->SetInt("Graphics","TextureQuality", mlTextureQuality);
	gpBase->mpMainConfig->SetInt("Graphics","TextureFilter", mlTextureFilter);
	gpBase->mpMainConfig->SetFloat("Graphics","TextureAnisotropy", mfTextureAnisotropy);

	gpBase->mpMainConfig->SetBool("Graphics","SSAOActive",mbSSAOActive);
	gpBase->mpMainConfig->SetInt("Graphics","SSAOResolution",mlSSAOResolution);
	gpBase->mpMainConfig->SetInt("Graphics","SSAOSamples",mlSSAOSamples);
	
	gpBase->mpMainConfig->SetBool("Graphics", "WorldReflection", mbWorldReflection);
	gpBase->mpMainConfig->SetBool("Graphics", "Refraction", mbRefraction);
	
	gpBase->mpMainConfig->SetBool("Graphics", "ShadowsActive", mbShadowsActive);
	gpBase->mpMainConfig->SetInt("Graphics","ShadowQuality", mlShadowQuality);
	gpBase->mpMainConfig->SetInt("Graphics","ShadowResolution", mlShadowRes);

	gpBase->mpMainConfig->SetInt("Graphics","ParallaxQuality", mlParallaxQuality);
	gpBase->mpMainConfig->SetBool("Graphics", "ParallaxEnabled", mbParallaxEnabled);

	gpBase->mpMainConfig->SetBool("Graphics", "EdgeSmooth", mbEdgeSmooth);

	gpBase->mpMainConfig->SetBool("Graphics", "ForceShaderModel3And4Off", mbForceShaderModel3And4Off);

	/////////////////////
	// Sound variables
	cSound *pSound = gpBase->mpEngine->GetSound();
	gpBase->mpMainConfig->SetInt("Sound", "Device", mlSoundDevID);
	gpBase->mpMainConfig->SetFloat("Sound","Volume", pSound->GetLowLevel()->GetVolume());
	gpBase->mpMainConfig->SetInt("Sound", "MaxChannels", mlMaxSoundChannels);
	gpBase->mpMainConfig->SetInt("Sound", "StreamBuffers", mlSoundStreamBuffers);
	gpBase->mpMainConfig->SetInt("Sound", "StreamBufferSize", mlSoundStreamBufferSize);

	/////////////////////
	// Engine properties
	gpBase->mpMainConfig->SetBool("Engine","LimitFPS", gpBase->mpEngine->GetLimitFPS());
	gpBase->mpMainConfig->SetBool("Engine","SleepWhenOutOfFocus",gpBase->mpEngine->GetWaitIfAppOutOfFocus());

	/////////////////////
	// Debug features
	gpBase->mpMainConfig->SetBool("Debug","ConsoleEnabled", mbDebugConsoleEnabled);
	gpBase->mpMainConfig->SetBool("Debug","DebugMenuEnabled", mbDebugMenuEnabled);
	gpBase->mpMainConfig->SetBool("Debug","VerboseLogging", mbVerboseLogging);
	gpBase->mpMainConfig->SetBool("Debug","WarningAccepted", mbDebugWarningAccepted);

	/////////////////////
	// Co-op
	//
	// The split mode is read back off the map handler rather than from our own
	// copy: the debug menu can change it live, and what the player is actually
	// playing with is what should still be there next launch.
	if(gpBase->mpMapHandler)
	{
		mlCoopSplitMode        = gpBase->mpMapHandler->GetSplitScreenMode();
		mlCoopDualMonitorIndex = gpBase->mpMapHandler->GetDualMonitorIndex();
	}

	gpBase->mpMainConfig->SetInt("Coop","SplitMode", mlCoopSplitMode);
	gpBase->mpMainConfig->SetInt("Coop","DualMonitorIndex", mlCoopDualMonitorIndex);

	gpBase->mpMainConfig->SetBool("Coop","AllowModelChange", mbCoopAllowModelChange);
	gpBase->mpMainConfig->SetBool("Coop","ModelWarningAccepted", mbCoopModelWarningAccepted);
	gpBase->mpMainConfig->SetInt("Coop","ModelP1", mlCoopModelP1);
	gpBase->mpMainConfig->SetInt("Coop","ModelP2", mlCoopModelP2);

	gpBase->mpMainConfig->SetBool("Coop","TeleportBothPlayers", mbCoopScriptTeleportBoth);
	gpBase->mpMainConfig->SetBool("Coop","SharedFlashbacks", mbCoopSharedFlashbacks);
	gpBase->mpMainConfig->SetBool("Coop","BothAtLevelDoor", mbCoopBothAtLevelDoor);
	gpBase->mpMainConfig->SetBool("Coop","SharedSanityReward", mbCoopSharedSanityReward);
	gpBase->mpMainConfig->SetBool("Coop","SharedLantern", mbCoopSharedLantern);
	gpBase->mpMainConfig->SetBool("Coop","SharedOilEffect", mbCoopShareOilEffect);
	gpBase->mpMainConfig->SetBool("Coop","SharedLaudanumEffect", mbCoopShareLaudanumEffect);
	gpBase->mpMainConfig->SetBool("Coop","SharedSanityPotionEffect", mbCoopShareSanityPotionEffect);
	gpBase->mpMainConfig->SetBool("Coop","SharedTinderboxes", mbCoopShareTinderboxes);
	gpBase->mpMainConfig->SetBool("Coop","Player2Callbacks", mbCoopP2Callbacks);
	gpBase->mpMainConfig->SetBool("Coop","FullItemShare", mbCoopFullItemShare);

	gpBase->mpMainConfig->SetBool("Coop","ForcedWarningAccepted", mbCoopForcedWarningAccepted);
}

//-----------------------------------------------------------------------

void cLuxConfigHandler::ApplyDebugAndCoopSettings()
{
	/////////////////////
	// Debug features
	ImGuiManager::SetConsoleEnabled(mbDebugConsoleEnabled);
	ImGuiManager::SetDebugMenuEnabled(mbDebugMenuEnabled);
	SetLogVerbose(mbVerboseLogging);

	/////////////////////
	// Co-op view
	if(gpBase->mpMapHandler)
	{
		gpBase->mpMapHandler->SetDualMonitorIndex(mlCoopDualMonitorIndex);
		gpBase->mpMapHandler->SetSplitScreenMode(mlCoopSplitMode);
	}

	/////////////////////
	// Co-op avatars
	ImGuiDebugMenu::SetAllowCoopModelChange(mbCoopAllowModelChange);
	ImGuiDebugMenu::SetCoopModelP1(mlCoopModelP1);
	ImGuiDebugMenu::SetCoopModelP2(mlCoopModelP2);

	/////////////////////
	// Forced-coop compatibility. Two are stored the way the menu states them
	// and inverted here -- see the header.
	ImGuiDebugMenu::SetScriptTeleportBothPlayers(mbCoopScriptTeleportBoth);
	ImGuiDebugMenu::SetSharedFlashbackVisions(mbCoopSharedFlashbacks);
	ImGuiDebugMenu::SetLevelDoorSinglePlayer(mbCoopBothAtLevelDoor==false);
	ImGuiDebugMenu::SetRewardBothSanity(mbCoopSharedSanityReward);
	ImGuiDebugMenu::SetLanternPerPlayer(mbCoopSharedLantern==false);
	ImGuiDebugMenu::SetShareOilEffect(mbCoopShareOilEffect);
	ImGuiDebugMenu::SetShareLaudanumEffect(mbCoopShareLaudanumEffect);
	ImGuiDebugMenu::SetShareSanityPotionEffect(mbCoopShareSanityPotionEffect);
	ImGuiDebugMenu::SetShareTinderboxes(mbCoopShareTinderboxes);
	ImGuiDebugMenu::SetP2AllCallbacks(mbCoopP2Callbacks);
	ImGuiDebugMenu::SetFullItemShare(mbCoopFullItemShare);

	//Enabling the shared pool mid-game leaves the two counts out of step.
	cLuxPlayer::CoopSyncSharedTinderboxes();
}

//-----------------------------------------------------------------------

bool cLuxConfigHandler::ShowRestartWarning(cGuiSet* apSet, void* apObject, tGuiCallbackFunc apCallback)
{
	///////////////////////////////////////////////////////////////////////////////////
	// This will show a popup if really needed, once per "restarting" setting changed.
	// Also, will return true if the popup was actually shown.
	if(mbGameNeedsRestart && mbRestartDialogShown==false)
	{
		mbRestartDialogShown = true;
		cGuiPopUpMessageBox* pPopUp = apSet->CreatePopUpMessageBox(kTranslate("OptionsMenu", "ReqRestartLabel"),
									 kTranslate("OptionsMenu", "ReqRestartMessage"), 
									 kTranslate("MainMenu","OK"), _W(""),
									 apObject, apCallback);
		pPopUp->GetGuiSet()->SetDrawFocus(true);
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------
