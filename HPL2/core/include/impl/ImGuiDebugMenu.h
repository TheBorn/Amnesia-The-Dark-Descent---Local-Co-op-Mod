#pragma once
#include <functional>
#include <map>
#include <string>

class ImGuiDebugMenu
{
public:
    static void Init();
    static void Shutdown();
    static void Draw();
    static void Toggle();
    static bool IsVisible();
    /** Force a state, used when the player switches the menu off in options. */
    static void SetVisible(bool abX);

    // Register game-side callbacks for coop toggle (called from game init, not engine)
    static void SetCoopCallbacks(std::function<bool()> aGetFn, std::function<void(bool)> aSetFn);

    // Split-screen mode and dual monitor
    static void SetSplitModeCallbacks(std::function<int()> aGetFn, std::function<void(int)> aSetFn);
    static void SetDualMonitorCallbacks(std::function<int()> aGetFn, std::function<void(int)> aSetFn);

    // ------------------------------------------------------------------
    // FORCED COOP -- not a toggle, a derived state supplied by the game:
    //
    //   0 = co-op off            compat options idle (armed for next time)
    //   1 = FORCED coop          co-op is running on content that never
    //                            declared co-op support (the main game, or a
    //                            custom story that did not declare SupportsCoop).
    //                            Every compat option below applies as set.
    //   2 = native co-op story   the story declared SupportsCoop="true" in its
    //                            custom_story_settings.cfg. It addresses each player
    //                            itself -- individually or through the first
    //                            caller -- so the whole compat layer steps
    //                            aside and each option is LOCKED to the value
    //                            that keeps the engine out of the story's way.
    static void SetForcedCoopStateCallback(std::function<int()> aFn);
    static int  GetForcedCoopState();
    static bool IsNativeCoopStory() { return GetForcedCoopState() == 2; }

    // ------------------------------------------------------------------
    // COOP OPTIONS -- story-owned values (the SetCoopAllow* script functions),
    // living game-side on the map handler and reached through callbacks.
    // Bools travel as 0.0/1.0. In a native coop story the getters below route
    // the relevant debug options through these; the Coop Options section of
    // the menu reads AND writes them live.
    enum eImGuiCoopOption
    {
        eImGuiCoopOption_CrouchBoost = 0,
        eImGuiCoopOption_PlayerCollision = 1,
        eImGuiCoopOption_MonsterKilling = 2,
        eImGuiCoopOption_MonsterDamageMul = 3,
        eImGuiCoopOption_GlobalLantern = 4
    };
    static void SetCoopOptionCallbacks(std::function<float(int)> aGetFn,
                                       std::function<void(int,float)> aSetFn);
    static float GetNativeCoopOption(int alOpt);            //0 when no callback
    static void  SetNativeCoopOption(int alOpt, float afVal);

    // --- Option accessors ---
    static bool GetAllowGrabSameObject()        { return mbAllowGrabSameObject; }
    static bool GetAllowCheats()                { return mbAllowCheats; }
    // Player-vs-player collision + crouch boosting, as chosen in the debug
    // menu. FORCED-COOP values only: cLuxMapHandler::CoopPlayerCollisionEffective /
    // CoopCrouchBoostEffective do the native-story routing, so these stay raw.
    static bool GetEnableCoopPlayerCollisions() { return mbEnableCoopPlayerCollisions; }
    static bool GetCoopCrouchBoost()            { return mbCoopCrouchBoost; }
    static bool GetCoopFlashbackTint()          { return mbCoopFlashbackTint; }
    static bool GetCoopPostEffects()            { return mbCoopPostEffects; }
    // 1.0 = render every view at full window size, which is what the engine has
    // always done. Lower renders into a sub-rect and scales up on the final blit.
    static float GetRenderScale()               { return mfRenderScale; }
    // Map-script callbacks Player 2 is allowed to trigger. Each type keeps a
    // single fired/colliding flag, so a trigger still fires ONCE however many
    // players are stood in it -- these decide whether P2 counts at all.
    //
    // FORCED COOP options: in a native co-op story the author registered the
    // callbacks knowing there are two players, so an unsuffixed callback means
    // "either player" -- locked TRUE there (per-player registrations exist for
    // when the story wants only one of them to count).
    static bool GetP2LookAtCallbacks()          { return IsNativeCoopStory() ? true  : mbP2LookAtCallbacks; }
    static bool GetP2InteractCallbacks()        { return IsNativeCoopStory() ? true  : mbP2InteractCallbacks; }
    static bool GetP2EnterCallbacks()           { return IsNativeCoopStory() ? true  : mbP2EnterCallbacks; }

    // Avatar options
    static bool GetCoopInvertBodyTilt()         { return mbCoopInvertBodyTilt; }
    static bool GetCoopShowLegs()               { return mbCoopShowLegs; }
    static bool GetCoopShowArms()               { return mbCoopShowArms; }
    static bool GetCoopP2Wig()                  { return mbCoopP2Wig; }

    // Which body each player is drawn as. 0 = the procedural stickman; the rest
    // index eAvatarModel in LuxPlayerAvatar.h. Read every frame, so a change here
    // swaps the model on the spot.
    //
    // Gated on GetAllowCoopModelChange: the rigged models are experimental (the
    // lantern-holding and grab animations do not work on them), so with the gate
    // off both players are the stickman whatever the two values below say.
    static bool GetAllowCoopModelChange()       { return mbAllowCoopModelChange; }
    static int GetCoopModelP1()                 { return mbAllowCoopModelChange ? mlCoopModelP1 : 0; }
    static int GetCoopModelP2()                 { return mbAllowCoopModelChange ? mlCoopModelP2 : 0; }
    // The raw stored choices, for the UI that edits them.
    static int GetCoopModelP1Raw()              { return mlCoopModelP1; }
    static int GetCoopModelP2Raw()              { return mlCoopModelP2; }

    // Degrees added on top of each model's table yaw, for finding the facing of
    // the three rigs whose .ent does not state one.
    static float GetCoopModelYawTrim()          { return mfCoopModelYawTrim; }

    // ------------------------------------------------------------------
    // VIEW AIDS
    //
    // Debug view is the level editor's view of the map: script areas and the
    // other invisible volumes drawn as labelled boxes. It also decides whether
    // the console's `delete` will touch an area -- you should not be able to
    // remove something you cannot see.
    //
    // Show colliders is the model editor's: every physics shape in the world
    // drawn as wireframe. Players are left out; you are inside your own.
    static bool GetDebugView()                  { return mbDebugView; }
    static void SetDebugView(bool abX)          { mbDebugView = abX; }
    static bool GetShowColliders()              { return mbShowColliders; }
    static void SetShowColliders(bool abX)      { mbShowColliders = abX; }

    // Water lurkers ship with a full model that the game never shows -- the
    // enemy forces its mesh invisible on load and again on every activation, so
    // all you ever see is the wake. The model is there and animates; this just
    // stops hiding it.
    static bool GetShowWaterLurkers()           { return mbShowWaterLurkers; }
    static void SetShowWaterLurkers(bool abX)   { mbShowWaterLurkers = abX; }

    // Teleport option (debug cheat -- not part of the forced-coop layer)
    static bool GetPlayerTeleport()             { return mbPlayerTeleport; }

    // FORCED COOP options. Each second value is what a native co-op story
    // locks the option to -- always the choice that leaves the story in
    // charge: no auto-dragging P2 on scripted teleports, no engine-side
    // sharing of items/effects (the story rewards whom it means to), lantern
    // per player, vanilla level-door triggering, no force-shared visions.
    static bool GetScriptTeleportBothPlayers()  { return IsNativeCoopStory() ? false : mbScriptTeleportBothPlayers; }
    static bool GetFullItemShare()              { return IsNativeCoopStory() ? false : mbFullItemShare; }
    static bool GetShareSanityPotionEffect()    { return IsNativeCoopStory() ? false : mbShareSanityPotionEffect; }
    static bool GetRewardBothSanity()           { return IsNativeCoopStory() ? false : mbRewardBothSanity; }
    static bool GetShareLaudanumEffect()        { return IsNativeCoopStory() ? false : mbShareLaudanumEffect; }
    static bool GetShareOilEffect()             { return IsNativeCoopStory() ? false : mbShareOilEffect; }
    // Native coop: the story's scripted Global Lantern choice (the flag's TRUE
    // semantics ARE the global lantern -- one found lantern serves both).
    static bool GetLanternPerPlayer()
    {
        if(IsNativeCoopStory()) return GetNativeCoopOption(eImGuiCoopOption_GlobalLantern) != 0.0f;
        return mbLanternPerPlayer;
    }
    static bool GetLevelDoorSinglePlayer()      { return IsNativeCoopStory() ? true  : mbLevelDoorSinglePlayer; }
    static bool GetSharedFlashbackVisions()     { return IsNativeCoopStory() ? false : mbSharedFlashbackVisions; }
    // One tinderbox pool for both players. Off, each player carries their own
    // count exactly as single player does. A native coop story counts its own
    // tinderboxes, so it is off there like every other engine-side share.
    static bool GetShareTinderboxes()           { return IsNativeCoopStory() ? false : mbShareTinderboxes; }

    // Monster manipulation. The master switch gates the other two: with it off
    // nothing here runs and monsters behave exactly as they always have.
    //
    // In a NATIVE coop story these route to the story's scripted Coop Options
    // (SetCoopAllowMonsterKilling / SetCoopMonsterPropDamageMul); the one-hit
    // cheat is debug-only and simply off there.
    static bool  GetAllowKillingMonsters()
    {
        if(IsNativeCoopStory()) return GetNativeCoopOption(eImGuiCoopOption_MonsterKilling) != 0.0f;
        return mbAllowKillingMonsters;
    }
    static float GetPropDamagePerKg()
    {
        if(IsNativeCoopStory()) return GetNativeCoopOption(eImGuiCoopOption_MonsterDamageMul);
        return mfPropDamagePerKg;
    }
    static bool  GetOneHitPropKill()            { return IsNativeCoopStory() ? false : mbOneHitPropKill; }

    // General cheats
    static bool GetFreeUseCandles()             { return mbFreeUseCandles; }
    static bool GetFreeUseLantern()             { return mbFreeUseLantern; }

    // Debug guns
    static bool  GetDebugGuns()                 { return mbDebugGuns; }

    // Possession. Turning this OFF releases whatever is held on the next frame
    // (cLuxPlayerPossess::Update checks it), so it doubles as a panic button.
    static bool  GetAllowPossession()           { return mbAllowPossession; }
    static float GetPossessCamDistance()        { return mfPossessCamDistance; }
    static float GetPossessCamHeight()          { return mfPossessCamHeight; }
    static float GetGunImpactForce()            { return mfGunImpactForce; }
    static float GetGunDamage()                 { return mfGunDamage; }

    // ------------------------------------------------------------------
    // SETTERS
    //
    // The options above started life as debug-menu-only statics. The game's own
    // Coop and Debug option tabs are now the front door for the ones a player is
    // meant to touch, and cLuxConfigHandler persists them across runs -- these
    // are how it writes them back in. Everything else stays debug-menu-only.
    static void SetEnableCoopPlayerCollisions(bool abX) { mbEnableCoopPlayerCollisions = abX; }
    static void SetCoopFlashbackTint(bool abX)      { mbCoopFlashbackTint = abX; }
    static void SetCoopPostEffects(bool abX)        { mbCoopPostEffects = abX; }
    static void SetRenderScale(float afX)           { mfRenderScale = afX; }

    // The three script-callback types Player 2 may trigger. The game UI drives
    // them as one option ("Player 2 triggers events"); the debug menu still
    // separates them.
    static void SetP2LookAtCallbacks(bool abX)      { mbP2LookAtCallbacks = abX; }
    static void SetP2InteractCallbacks(bool abX)    { mbP2InteractCallbacks = abX; }
    static void SetP2EnterCallbacks(bool abX)       { mbP2EnterCallbacks = abX; }
    static void SetP2AllCallbacks(bool abX)
    {
        mbP2LookAtCallbacks   = abX;
        mbP2InteractCallbacks = abX;
        mbP2EnterCallbacks    = abX;
    }
    static bool GetP2AllCallbacksRaw()
    {
        return mbP2LookAtCallbacks && mbP2InteractCallbacks && mbP2EnterCallbacks;
    }

    static void SetCoopInvertBodyTilt(bool abX)     { mbCoopInvertBodyTilt = abX; }
    static void SetCoopShowLegs(bool abX)           { mbCoopShowLegs = abX; }
    static void SetCoopShowArms(bool abX)           { mbCoopShowArms = abX; }
    static void SetCoopP2Wig(bool abX)              { mbCoopP2Wig = abX; }
    static void SetAllowCoopModelChange(bool abX)   { mbAllowCoopModelChange = abX; }
    static void SetCoopModelP1(int alX)             { mlCoopModelP1 = alX; }
    static void SetCoopModelP2(int alX)             { mlCoopModelP2 = alX; }
    static void SetCoopModelYawTrim(float afX)      { mfCoopModelYawTrim = afX; }

    static void SetPlayerTeleport(bool abX)         { mbPlayerTeleport = abX; }
    static void SetScriptTeleportBothPlayers(bool abX) { mbScriptTeleportBothPlayers = abX; }
    static void SetFullItemShare(bool abX)          { mbFullItemShare = abX; }
    static void SetShareSanityPotionEffect(bool abX){ mbShareSanityPotionEffect = abX; }
    static void SetRewardBothSanity(bool abX)       { mbRewardBothSanity = abX; }
    static void SetShareLaudanumEffect(bool abX)    { mbShareLaudanumEffect = abX; }
    static void SetShareOilEffect(bool abX)         { mbShareOilEffect = abX; }
    static void SetLanternPerPlayer(bool abX)       { mbLanternPerPlayer = abX; }
    static void SetLevelDoorSinglePlayer(bool abX)  { mbLevelDoorSinglePlayer = abX; }
    static void SetSharedFlashbackVisions(bool abX) { mbSharedFlashbackVisions = abX; }
    static void SetShareTinderboxes(bool abX)       { mbShareTinderboxes = abX; }

    // Raw reads, bypassing the native-coop-story routing the getters above do.
    // The options UI must show and write what the PLAYER chose, not what the
    // story currently overrides it to.
    static bool GetScriptTeleportBothPlayersRaw()   { return mbScriptTeleportBothPlayers; }
    static bool GetFullItemShareRaw()               { return mbFullItemShare; }
    static bool GetShareSanityPotionEffectRaw()     { return mbShareSanityPotionEffect; }
    static bool GetRewardBothSanityRaw()            { return mbRewardBothSanity; }
    static bool GetShareLaudanumEffectRaw()         { return mbShareLaudanumEffect; }
    static bool GetShareOilEffectRaw()              { return mbShareOilEffect; }
    static bool GetLanternPerPlayerRaw()            { return mbLanternPerPlayer; }
    static bool GetLevelDoorSinglePlayerRaw()       { return mbLevelDoorSinglePlayer; }
    static bool GetSharedFlashbackVisionsRaw()      { return mbSharedFlashbackVisions; }
    static bool GetShareTinderboxesRaw()            { return mbShareTinderboxes; }

    // --- Render diagnostics (written by engine code, read-only readout) ---
    /**
     * Frame profiler. Call once per module per frame with how long that
     * module's Update took. Averaged over a one-second window for display,
     * with a running peak kept separately so a spike is not averaged away.
     */
    static void AddDiagModuleTime(const char *asName, float afMs);

    static void AddDiagRenderWork(int alLights, int alQueries)
    {
        mlDiagLightsAccum  += alLights;
        mlDiagQueriesAccum += alQueries;
    }
    static void SetDiagViewportCounts(int alVisible, int alTotal)
    {
        mlDiagVisibleViewports = alVisible;
        mlDiagTotalViewports   = alTotal;
    }
    static void AddDiagShadowMapRender()        { ++mlDiagShadowMapRendersAccum; }

    /**
     * One line per co-op avatar, pushed by the game layer every frame.
     *
     * Exists because an invisible player has several possible causes that all
     * look identical from outside -- parts never built, built for a world that
     * is no longer current, no character body, or simply never asked to show.
     * Reading it off the screen beats inferring it.
     */
    static void SetDiagAvatarLine(int alPlayer, const char *asText)
    {
        if(alPlayer < 0 || alPlayer > 1 || asText == NULL) return;
        msDiagAvatar[alPlayer] = asText;
    }
    static void LatchDiagFrameCounters()
    {
        mlDiagShadowMapRenders      = mlDiagShadowMapRendersAccum;
        mlDiagShadowMapRendersAccum = 0;
        mlDiagLights                = mlDiagLightsAccum;
        mlDiagLightsAccum           = 0;
        mlDiagQueries               = mlDiagQueriesAccum;
        mlDiagQueriesAccum          = 0;

        // Roll the profiler window about once a second at 60fps.
        ++mlDiagProfileFrames;
        if(mlDiagProfileFrames >= 60)
        {
            mMapDiagModuleAvg.clear();
            for(std::map<std::string, float>::iterator it = mMapDiagModuleAccum.begin();
                it != mMapDiagModuleAccum.end(); ++it)
            {
                mMapDiagModuleAvg[it->first] = it->second / (float)mlDiagProfileFrames;
            }
            mMapDiagModuleAccum.clear();
            mlDiagProfileFrames = 0;
        }
    }

    static void ResetDiagProfilePeaks() { mMapDiagModulePeak.clear(); }

private:
    static bool mbVisible;

    // Debug options
    static bool mbAllowGrabSameObject;
    static bool mbAllowCheats;
    static bool mbEnableCoopPlayerCollisions;
    static bool mbCoopFlashbackTint;
    static bool mbCoopPostEffects;
    static float mfRenderScale;
    static bool mbP2LookAtCallbacks;
    static bool mbP2InteractCallbacks;
    static bool mbP2EnterCallbacks;

    // Avatar options
    static bool mbCoopInvertBodyTilt;
    static bool mbCoopShowLegs;
    static bool mbCoopShowArms;
    static bool mbCoopP2Wig;
    static int  mlCoopModelP1;
    static int  mlCoopModelP2;
    static bool mbAllowCoopModelChange;
    static float mfCoopModelYawTrim;

    // View aids
    static bool mbDebugView;
    static bool mbShowColliders;
    static bool mbShowWaterLurkers;

    // Teleport option
    static bool mbPlayerTeleport;
    static bool mbScriptTeleportBothPlayers;

    // Shared inventory options
    static bool mbFullItemShare;
    static bool mbShareSanityPotionEffect;
    static bool mbRewardBothSanity;
    static bool mbShareLaudanumEffect;
    static bool mbShareOilEffect;
    static bool mbLanternPerPlayer;
    static bool mbShareTinderboxes;

    // World / progression options
    static bool mbLevelDoorSinglePlayer;
    static bool mbSharedFlashbackVisions;

    // Monster manipulation
    static bool  mbAllowKillingMonsters;
    static float mfPropDamagePerKg;
    static bool  mbOneHitPropKill;

    // General cheats
    static bool mbFreeUseCandles;
    static bool mbFreeUseLantern;

    // Debug guns
    static bool  mbDebugGuns;
    static bool  mbAllowPossession;
    static float mfPossessCamDistance;
    static float mfPossessCamHeight;
    static float mfGunImpactForce;
    static float mfGunDamage;

    // Render diagnostics
    static int mlDiagVisibleViewports;
    static int mlDiagTotalViewports;
    static int mlDiagShadowMapRenders;
    static int mlDiagShadowMapRendersAccum;
    static int mlDiagLights;
    static int mlDiagLightsAccum;
    static int mlDiagQueries;
    static int mlDiagQueriesAccum;

    // Co-op avatar state, one line per player. See SetDiagAvatarLine above.
    static std::string msDiagAvatar[2];

    // Frame profiler
    static std::map<std::string, float> mMapDiagModuleAccum;
    static std::map<std::string, float> mMapDiagModuleAvg;
    static std::map<std::string, float> mMapDiagModulePeak;
    static int mlDiagProfileFrames;

    // Coop callbacks
    static std::function<bool()>     mGetCoopFn;
    static std::function<void(bool)> mSetCoopFn;

    // Split mode callbacks
    static std::function<int()>      mGetSplitModeFn;
    static std::function<void(int)>  mSetSplitModeFn;

    // Dual monitor callbacks
    static std::function<int()>      mGetDualMonitorFn;
    static std::function<void(int)>  mSetDualMonitorFn;

    // Forced-coop state callback (see GetForcedCoopState above)
    static std::function<int()>      mGetForcedCoopStateFn;

    // Coop Options callbacks (story-owned values on the map handler)
    static std::function<float(int)>      mGetCoopOptionFn;
    static std::function<void(int,float)> mSetCoopOptionFn;

    // Debug toggle for the crouch-boost head collider (forced coop only;
    // native stories script it). Sits beside mbEnableCoopPlayerCollisions.
    static bool mbCoopCrouchBoost;
};