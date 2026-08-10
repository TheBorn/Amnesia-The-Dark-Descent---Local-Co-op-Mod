#include "impl/ImGuiDebugMenu.h"
#include "impl/LowLevelInputSDL.h"
#include "imgui/imgui.h"
#include "SDL2/SDL.h"
#include <vector>
#include <string>
#include <cstdio>

// ----------------------------------------------------------------
// Static members
// ----------------------------------------------------------------
bool ImGuiDebugMenu::mbVisible                   = false;
bool ImGuiDebugMenu::mbAllowGrabSameObject       = true;
bool ImGuiDebugMenu::mbAllowCheats               = true;
//False now that it actually DOES something (it shipped as an unimplemented
//placeholder defaulted true) -- switching the default on with the feature
//would change how every existing forced-coop session feels.
bool ImGuiDebugMenu::mbEnableCoopPlayerCollisions = false;
bool ImGuiDebugMenu::mbCoopFlashbackTint          = true;
bool ImGuiDebugMenu::mbCoopPostEffects            = true;
bool  ImGuiDebugMenu::mbAllowKillingMonsters     = false;
float ImGuiDebugMenu::mfPropDamagePerKg          = 10.0f;
bool  ImGuiDebugMenu::mbOneHitPropKill           = false;
bool ImGuiDebugMenu::mbFreeUseCandles             = false;
bool ImGuiDebugMenu::mbFreeUseLantern             = false;
bool  ImGuiDebugMenu::mbDebugGuns                 = false;
bool  ImGuiDebugMenu::mbAllowPossession           = false;
float ImGuiDebugMenu::mfPossessCamDistance        = 2.6f;
float ImGuiDebugMenu::mfPossessCamHeight          = 1.15f;
bool  ImGuiDebugMenu::mbAllowEnemyMorph           = false;
int   ImGuiDebugMenu::mlEnemyMorphRequest         = -1;
int   ImGuiDebugMenu::mlP2InputSource             = 0;
void *ImGuiDebugMenu::mpP2RawDevice               = 0;
void *ImGuiDebugMenu::mpP2RawMouse               = 0;
bool  ImGuiDebugMenu::mbP2RawMouseUserSet        = false;

int   ImGuiDebugMenu::mlDiagBlackViewportCount     = 0;
int   ImGuiDebugMenu::mlDiagBlackViewportLastFrame = -1;
char  ImGuiDebugMenu::msDiagBlackViewport[160]     = {0};
float ImGuiDebugMenu::mfGunImpactForce            = 5.0f;
float ImGuiDebugMenu::mfGunDamage                 = 25.0f;
float ImGuiDebugMenu::mfRenderScale               = 1.0f;
bool ImGuiDebugMenu::mbP2LookAtCallbacks          = true;
bool ImGuiDebugMenu::mbP2InteractCallbacks        = true;
bool ImGuiDebugMenu::mbP2EnterCallbacks           = true;

bool ImGuiDebugMenu::mbCoopInvertBodyTilt         = false;
bool ImGuiDebugMenu::mbCoopShowLegs               = true;
bool ImGuiDebugMenu::mbCoopShowArms               = true;
int  ImGuiDebugMenu::mlCoopModelP1                = 0;
int  ImGuiDebugMenu::mlCoopModelP2                = 0;
bool ImGuiDebugMenu::mbAllowCoopModelChange       = false;
float ImGuiDebugMenu::mfCoopModelYawTrim          = 0.0f;
bool ImGuiDebugMenu::mbCoopP2Wig                  = true;

bool ImGuiDebugMenu::mbDebugView                  = false;
bool ImGuiDebugMenu::mbShowColliders              = false;
bool ImGuiDebugMenu::mbShowWaterLurkers           = false;
bool ImGuiDebugMenu::mbPlayerTeleport             = true;
bool ImGuiDebugMenu::mbScriptTeleportBothPlayers  = true;
bool ImGuiDebugMenu::mbFullItemShare             = false;
bool ImGuiDebugMenu::mbShareSanityPotionEffect   = true;
bool ImGuiDebugMenu::mbRewardBothSanity          = true;
bool ImGuiDebugMenu::mbShareLaudanumEffect       = true;
bool ImGuiDebugMenu::mbShareOilEffect            = true;
bool ImGuiDebugMenu::mbLanternPerPlayer          = true;
bool ImGuiDebugMenu::mbShareTinderboxes          = false;
bool ImGuiDebugMenu::mbLevelDoorSinglePlayer     = true;
bool ImGuiDebugMenu::mbSharedFlashbackVisions    = true;

int  ImGuiDebugMenu::mlDiagVisibleViewports      = 0;
int  ImGuiDebugMenu::mlDiagTotalViewports        = 0;
int  ImGuiDebugMenu::mlDiagShadowMapRenders      = 0;
int  ImGuiDebugMenu::mlDiagShadowMapRendersAccum = 0;
std::string ImGuiDebugMenu::msDiagAvatar[2];
int  ImGuiDebugMenu::mlDiagLights                = 0;
int  ImGuiDebugMenu::mlDiagLightsAccum           = 0;
int  ImGuiDebugMenu::mlDiagQueries               = 0;
int  ImGuiDebugMenu::mlDiagQueriesAccum          = 0;

std::map<std::string, float> ImGuiDebugMenu::mMapDiagModuleAccum;
std::map<std::string, float> ImGuiDebugMenu::mMapDiagModuleAvg;
std::map<std::string, float> ImGuiDebugMenu::mMapDiagModulePeak;
int  ImGuiDebugMenu::mlDiagProfileFrames         = 0;

void ImGuiDebugMenu::AddDiagModuleTime(const char *asName, float afMs)
{
    if(asName == 0) return;

    mMapDiagModuleAccum[asName] += afMs;

    // Peak is per-FRAME, not per-window: a 30ms hitch in one module is the
    // whole point and must not be averaged into invisibility.
    float &fPeak = mMapDiagModulePeak[asName];
    if(afMs > fPeak) fPeak = afMs;
}

std::function<bool()>     ImGuiDebugMenu::mGetCoopFn  = nullptr;
std::function<void(bool)> ImGuiDebugMenu::mSetCoopFn  = nullptr;

std::function<int()>      ImGuiDebugMenu::mGetSplitModeFn  = nullptr;
std::function<void(int)>  ImGuiDebugMenu::mSetSplitModeFn  = nullptr;

std::function<int()>      ImGuiDebugMenu::mGetDualMonitorFn  = nullptr;
std::function<void(int)>  ImGuiDebugMenu::mSetDualMonitorFn  = nullptr;

std::function<int()>      ImGuiDebugMenu::mGetForcedCoopStateFn = nullptr;
std::function<std::string()> ImGuiDebugMenu::mGetCoopDiagFn = nullptr;

std::function<float(int)>      ImGuiDebugMenu::mGetCoopOptionFn = nullptr;
std::function<void(int,float)> ImGuiDebugMenu::mSetCoopOptionFn = nullptr;

bool ImGuiDebugMenu::mbCoopCrouchBoost = true;

void ImGuiDebugMenu::SetCoopDiagCallback(std::function<std::string()> aFn)
{
    mGetCoopDiagFn = std::move(aFn);
}

std::string ImGuiDebugMenu::GetCoopDiag()
{
    if (!mGetCoopDiagFn) return std::string("(no callback)");
    return mGetCoopDiagFn();
}

void ImGuiDebugMenu::SetForcedCoopStateCallback(std::function<int()> aFn)
{
    mGetForcedCoopStateFn = std::move(aFn);
}

int ImGuiDebugMenu::GetForcedCoopState()
{
    //No callback yet (early init, editors): report co-op off, which leaves
    //every compat getter returning its raw toggle -- the pre-reorg behaviour.
    if (!mGetForcedCoopStateFn) return 0;
    return mGetForcedCoopStateFn();
}

void ImGuiDebugMenu::SetCoopOptionCallbacks(std::function<float(int)> aGetFn,
                                            std::function<void(int,float)> aSetFn)
{
    mGetCoopOptionFn = std::move(aGetFn);
    mSetCoopOptionFn = std::move(aSetFn);
}

float ImGuiDebugMenu::GetNativeCoopOption(int alOpt)
{
    if (!mGetCoopOptionFn) return 0.0f;
    return mGetCoopOptionFn(alOpt);
}

void ImGuiDebugMenu::SetNativeCoopOption(int alOpt, float afVal)
{
    if (!mSetCoopOptionFn) return;
    mSetCoopOptionFn(alOpt, afVal);
}

// ----------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------
static void HelpMarker(const char* desc)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 22.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// ----------------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------------
void ImGuiDebugMenu::Init()
{
    mbVisible = false;
}

void ImGuiDebugMenu::Shutdown()
{
    mbVisible = false;
    mGetCoopFn = nullptr;
    mSetCoopFn = nullptr;
    mGetSplitModeFn = nullptr;
    mSetSplitModeFn = nullptr;
    mGetDualMonitorFn = nullptr;
    mSetDualMonitorFn = nullptr;
    mGetForcedCoopStateFn = nullptr;
    mGetCoopDiagFn = nullptr;
    mGetCoopOptionFn = nullptr;
    mSetCoopOptionFn = nullptr;
}

void ImGuiDebugMenu::SetCoopCallbacks(std::function<bool()> aGetFn, std::function<void(bool)> aSetFn)
{
    mGetCoopFn = std::move(aGetFn);
    mSetCoopFn = std::move(aSetFn);
}

void ImGuiDebugMenu::SetSplitModeCallbacks(std::function<int()> aGetFn, std::function<void(int)> aSetFn)
{
    mGetSplitModeFn = std::move(aGetFn);
    mSetSplitModeFn = std::move(aSetFn);
}

void ImGuiDebugMenu::SetDualMonitorCallbacks(std::function<int()> aGetFn, std::function<void(int)> aSetFn)
{
    mGetDualMonitorFn = std::move(aGetFn);
    mSetDualMonitorFn = std::move(aSetFn);
}

void ImGuiDebugMenu::SetVisible(bool abX)
{
    if (mbVisible == abX) return;

    if (abX)
    {
        Toggle();
        return;
    }

    //Hiding deliberately does NOT run Toggle's cursor restore. This is called
    //from the options menu, where re-grabbing the mouse into relative mode is
    //exactly wrong -- the player is still pointing at widgets. Gameplay
    //re-asserts its own cursor state when it resumes.
    mbVisible = false;
}

void ImGuiDebugMenu::Toggle()
{
    mbVisible = !mbVisible;

    if (mbVisible)
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_ShowCursor(SDL_ENABLE);
    }
    else
    {
        SDL_ShowCursor(SDL_DISABLE);
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
}

bool ImGuiDebugMenu::IsVisible()
{
    return mbVisible;
}

// ----------------------------------------------------------------
// Draw
// ----------------------------------------------------------------
void ImGuiDebugMenu::Draw()
{
    if (!mbVisible) return;

    const ImGuiIO& io = ImGui::GetIO();

    // --- Palette (console-adjacent dark olive/amber theme) ---
    const ImVec4 colWinBg       = ImVec4(0.10f, 0.10f, 0.08f, 0.96f);
    const ImVec4 colTitleBg     = ImVec4(0.16f, 0.16f, 0.10f, 1.00f);
    const ImVec4 colTitleActive = ImVec4(0.24f, 0.22f, 0.12f, 1.00f);
    const ImVec4 colFrameBg     = ImVec4(0.16f, 0.16f, 0.12f, 1.00f);
    const ImVec4 colFrameHover  = ImVec4(0.24f, 0.22f, 0.16f, 1.00f);
    const ImVec4 colCheckMark   = ImVec4(1.00f, 1.00f, 0.00f, 1.00f);
    const ImVec4 colHeader      = ImVec4(0.18f, 0.18f, 0.12f, 1.00f);
    const ImVec4 colHeaderHov   = ImVec4(0.28f, 0.26f, 0.16f, 1.00f);
    const ImVec4 colSeparator   = ImVec4(0.40f, 0.40f, 0.20f, 0.50f);
    const ImVec4 colBorder      = ImVec4(0.30f, 0.30f, 0.18f, 0.80f);
    const ImVec4 colButton      = ImVec4(0.20f, 0.20f, 0.14f, 1.00f);
    const ImVec4 colButtonHov   = ImVec4(0.32f, 0.30f, 0.18f, 1.00f);
    const ImVec4 colButtonAct   = ImVec4(0.40f, 0.38f, 0.20f, 1.00f);
    const ImVec4 colSectionText = ImVec4(1.00f, 1.00f, 0.00f, 1.00f);
    const ImVec4 colNIText      = ImVec4(0.50f, 0.50f, 0.40f, 1.00f);

    // Push style
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,    3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(16.0f, 12.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,      ImVec2(10.0f, 7.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg,         colWinBg);
    ImGui::PushStyleColor(ImGuiCol_TitleBg,          colTitleBg);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,    colTitleActive);
    ImGui::PushStyleColor(ImGuiCol_Border,           colBorder);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,          colFrameBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   colFrameHover);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,    colFrameHover);
    ImGui::PushStyleColor(ImGuiCol_CheckMark,        colCheckMark);
    ImGui::PushStyleColor(ImGuiCol_Header,           colHeader);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,    colHeaderHov);
    ImGui::PushStyleColor(ImGuiCol_Separator,        colSeparator);
    ImGui::PushStyleColor(ImGuiCol_Button,           colButton);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,    colButtonHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,     colButtonAct);

    // Window setup — centered on first use
    ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

    const ImGuiWindowFlags winFlags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("Debug Options##HPL2Debug", &mbVisible, winFlags))
    {
        // ---- LEFT COLUMN ----------------------------------------------
        // Groups rather than ImGui::Columns or BeginTable: Columns needs a
        // content width and this window is AlwaysAutoResize, which is circular,
        // and BeginTable is newer than I can confirm this ImGui is. A group
        // measures itself, so SameLine below just puts the next one beside it
        // and the window grows to fit.
        ImGui::BeginGroup();

        // =============================================
        //  CO-OP
        // =============================================
        ImGui::TextColored(colSectionText, "CO-OP");
        ImGui::Separator();
        ImGui::Spacing();

        bool bCoop = mGetCoopFn ? mGetCoopFn() : false;
        if (ImGui::Checkbox("Enable Co-Op", &bCoop))
        {
            if (mSetCoopFn) mSetCoopFn(bCoop);
        }
        HelpMarker("Toggle split-screen co-op mode.\nSame as 'coop 1' / 'coop 0' in console.");

        // Split-screen mode dropdown
        {
            int iCurrentMode = mGetSplitModeFn ? mGetSplitModeFn() : 1;
            int iCurrentMonitor = mGetDualMonitorFn ? mGetDualMonitorFn() : -1;

            // Build dropdown items: first two fixed, then one per extra display
            const int iNumDisplays = SDL_GetNumVideoDisplays();

            // Detect which display the game window is on
            int iMainDisplay = 0;
            SDL_Window* pWin = SDL_GL_GetCurrentWindow();
            if (pWin) iMainDisplay = SDL_GetWindowDisplayIndex(pWin);

            std::vector<std::string> vLabels;
            std::vector<int> vDisplayIndices;

            vLabels.push_back("Left \xe2\x86\x92 Right");
            vDisplayIndices.push_back(-1);

            vLabels.push_back("Top \xe2\x86\x92 Bottom");
            vDisplayIndices.push_back(-1);

            for (int i = 0; i < iNumDisplays; i++)
            {
                if (i == iMainDisplay) continue;

                const char* name = SDL_GetDisplayName(i);
                SDL_DisplayMode mode;
                SDL_GetCurrentDisplayMode(i, &mode);

                char buf[256];
                snprintf(buf, sizeof(buf), "%s  %dx%d @%dHz",
                    name ? name : "Unknown", mode.w, mode.h, mode.refresh_rate);
                vLabels.push_back(buf);
                vDisplayIndices.push_back(i);
            }

            // Determine current combo selection
            int iComboIdx = 0;
            if (iCurrentMode == 2) iComboIdx = 1;
            else if (iCurrentMode == 3)
            {
                for (int i = 2; i < (int)vLabels.size(); i++)
                {
                    if (vDisplayIndices[i] == iCurrentMonitor)
                    {
                        iComboIdx = i;
                        break;
                    }
                }
                if (iComboIdx == 0 && (int)vLabels.size() > 2) iComboIdx = 2;
            }

            std::vector<const char*> vLabelPtrs;
            for (auto& s : vLabels) vLabelPtrs.push_back(s.c_str());

            if (ImGui::Combo("Split Layout", &iComboIdx, vLabelPtrs.data(), (int)vLabelPtrs.size()))
            {
                if (iComboIdx == 0)
                {
                    if (mSetSplitModeFn) mSetSplitModeFn(1);
                }
                else if (iComboIdx == 1)
                {
                    if (mSetSplitModeFn) mSetSplitModeFn(2);
                }
                else
                {
                    int iDispIdx = vDisplayIndices[iComboIdx];
                    if (mSetDualMonitorFn) mSetDualMonitorFn(iDispIdx);
                    if (mSetSplitModeFn) mSetSplitModeFn(3);
                }
            }
            HelpMarker("Choose split-screen layout.\nMonitor entries show connected displays\nfor dual-monitor co-op.");

            if (iNumDisplays <= 1)
            {
                ImGui::TextColored(colNIText, "  No additional monitors detected");
            }
        }

        ImGui::SliderFloat("Internal Render Scale", &mfRenderScale, 0.25f, 1.0f, "%.2f");
        HelpMarker("PERFORMANCE. The deferred renderer keeps ONE set of buffers, all\n"
                    "sized to the whole window, and every pass fills them. A split\n"
                    "view therefore costs a WHOLE frame, not half of one -- co-op is\n"
                    "two full frames however small the two views are.\n\n"
                    "This renders into a sub-rect of those buffers and scales up on\n"
                    "the final blit. 0.71 is about half the pixels, 0.50 a quarter.\n\n"
                    "1.00 is the old behaviour exactly -- every calculation collapses\n"
                    "back to the line it replaced. Put it back to 1.00 if anything\n"
                    "renders wrong and nothing else changes.");

        ImGui::Checkbox("Split-Screen Post Effects", &mbCoopPostEffects);
        HelpMarker("Give each split view its own post-effect pass, so bloom and\n"
                    "flashback sepia / radial blur work for BOTH players.\n\n"
                    "Automatically disabled in DUAL MONITOR layout: the window\n"
                    "spans two screens but the engine still reports one, so P2's\n"
                    "half samples past the edge of the render texture and comes\n"
                    "out black and banded. Left/Right and Top/Bottom are fine.");

        ImGui::Checkbox("Flashback Tint In Split-Screen", &mbCoopFlashbackTint);
        HelpMarker("Fallback for when the post pass is off (dual monitor, or the\n"
                    "option above unchecked): paints the flashback sepia straight\n"
                    "onto each player's HUD so both still see the vision.\n"
                    "Skipped automatically when the real post effects are running.");

        ImGui::Checkbox("Enable Player Collisions", &mbEnableCoopPlayerCollisions);
        HelpMarker("FULL player-vs-player collision, at all times -- not just from\n"
                    "above. Squeezing past each other in a corridor gets harder,\n"
                    "which some groups prefer. Implies head-standing too.\n\n"
                    "In a native co-op story this toggle is ignored; the story\n"
                    "scripts it via SetCoopAllowPlayerCollision (see Coop Options).");

        ImGui::Checkbox("Crouch Boost (Head Stand)", &mbCoopCrouchBoost);
        HelpMarker("Land on a CROUCHED partner's head from above and it is real,\n"
                    "standable ground -- they can then stand up underneath you and\n"
                    "boost you Counter-Strike style, and you can jump off.\n\n"
                    "Only contact from above counts: walking into each other, or\n"
                    "not coming down high enough, passes straight through as\n"
                    "always. Redundant while full player collisions are on.\n\n"
                    "In a native co-op story this toggle is ignored; the story\n"
                    "scripts it via SetCoopAllowCrouchBoost (see Coop Options).");

        ImGui::Spacing();
        ImGui::Spacing();

        // =============================================
        //  FORCED COOP  (state derived from the story -- never a toggle)
        // =============================================
        {
            const int lFC = GetForcedCoopState();

            ImGui::TextColored(colSectionText, "FORCED COOP");
            HelpMarker("The compatibility layer that forces co-op behaviour onto\n"
                        "content never written for two players: who may trigger the\n"
                        "story's callbacks, what gets shared, what needs both players.\n\n"
                        "The state line below is NOT a toggle -- it is decided by the\n"
                        "running story. A story that declares SupportsCoop in its\n"
                        "custom_story_settings.cfg\n"
                        "in OnGameStart is a NATIVE co-op story: it addresses each\n"
                        "player itself (individually or through the first caller), so\n"
                        "this whole layer steps aside and every option here locks to\n"
                        "its story-managed value. Everything else -- the main game\n"
                        "included -- runs under Forced Coop, where these apply as set.");
            ImGui::Separator();
            ImGui::Spacing();

            if (lFC == 1)
            {
                ImGui::TextColored(ImVec4(1.00f, 0.75f, 0.20f, 1.00f),
                                   "ACTIVE -- story does not declare co-op support");
            }
            else if (lFC == 2)
            {
                ImGui::TextColored(ImVec4(0.45f, 0.95f, 0.45f, 1.00f),
                                   "BYPASSED -- native co-op story is in control");
            }
            else
            {
                ImGui::TextColored(colNIText,
                                   "Idle -- co-op is off (applies when forced co-op runs)");
            }
            ImGui::Spacing();

            if (lFC == 2)
            {
                // Locked. Show the effective values the native story pins; the
                // raw toggles keep their state for the next forced-coop session.
                ImGui::TextColored(colNIText, "  P2 triggers look-at/interact/enter : ON");
                ImGui::TextColored(colNIText, "  Script teleports drag P2 along     : OFF");
                ImGui::TextColored(colNIText, "  Item / potion / oil / sanity share : OFF");
                ImGui::TextColored(colNIText, "  Lantern / collisions / monsters    : see COOP OPTIONS");
                ImGui::TextColored(colNIText, "  Level door needs one player only   : ON");
                ImGui::TextColored(colNIText, "  Shared flashback visions           : OFF");
                ImGui::TextColored(colNIText, "  The story drives both players itself.");
            }
            else
            {
                // ---- Script triggers ------------------------------------
                ImGui::Checkbox("P2 Triggers Look-At Callbacks", &mbP2LookAtCallbacks);
                HelpMarker("Let Player 2 set off a map script's PlayerLookAtCallback\n"
                            "just by looking at something.\n\n"
                            "Fires ONCE: whoever looks first wins, and the leave edge\n"
                            "waits until neither player is looking. If P2 is the one who\n"
                            "looked, P2 gets whatever the script does to 'the player'.");

                ImGui::Checkbox("P2 Triggers Interact Callbacks", &mbP2InteractCallbacks);
                HelpMarker("Let Player 2 set off a map script's PlayerInteractCallback\n"
                            "by using an object. This is how the game already behaved --\n"
                            "untick it if a story event you only want once is firing\n"
                            "again when P2 touches the same thing.");

                ImGui::Checkbox("P2 Triggers Enter/Leave Callbacks", &mbP2EnterCallbacks);
                HelpMarker("Let Player 2 set off script trigger volumes by walking into\n"
                            "them (AddEntityCollideCallback).\n\n"
                            "Fires ONCE on the FIRST player in and once on the LAST player\n"
                            "out, so standing in a trigger together cannot double-fire it.\n"
                            "One-shot triggers still delete themselves as before.");

                ImGui::Checkbox("Script Teleports Move Both Players", &mbScriptTeleportBothPlayers);
                HelpMarker("Map scripts reposition the player during cutscenes and\n"
                            "set-pieces, but only ever move Player 1 -- Player 2 gets\n"
                            "left behind in the previous room. Turn this on so P2 is\n"
                            "dragged along to P1 whenever a script moves P1.\n"
                            "(On by default.)");

                // ---- Progression ----------------------------------------
                ImGui::Checkbox("Level Doors Only Requires One Player Present", &mbLevelDoorSinglePlayer);
                HelpMarker("Normally a level door needs BOTH players within interaction\n"
                            "radius, to encourage both reaching the level's end before\n"
                            "switching. Turn this on so a single player at a level door\n"
                            "is enough.");

                ImGui::Checkbox("Shared Flashback Visions", &mbSharedFlashbackVisions);
                HelpMarker("Let both players see the same flashback vision at the same\n"
                            "time. These are generally safe to force-show for both\n"
                            "players when they trigger. (On by default.)");

                // ---- Sharing --------------------------------------------
                ImGui::Checkbox("Full Item Share", &mbFullItemShare);
                HelpMarker("All items are saved to one shared inventory and either\n"
                            "player can make use of them. You cannot give each other\n"
                            "items while this is on (no need to). Lantern options are\n"
                            "separate from this toggle.");

                ImGui::Checkbox("Share Sanity Potion Effect", &mbShareSanityPotionEffect);
                HelpMarker("If a player drinks a sanity potion, should BOTH players\n"
                            "receive the effect, or just the drinker themselves?");

                ImGui::Checkbox("Reward Both Players With Sanity Boosts", &mbRewardBothSanity);
                HelpMarker("Sanity rewards handed out by MAP SCRIPTS -- solving a puzzle,\n"
                            "using an item on the right thing, reaching a set-piece.\n\n"
                            "On: both players are rewarded. Each one's boost is worked\n"
                            "out from their OWN sanity, so a player who is already fine\n"
                            "does not get topped up to full.\n\n"
                            "Off: only the player who actually earned it.\n\n"
                            "Penalties are never shared, only rewards. (On by default.)");

                ImGui::Checkbox("Shared Laudanum Effect", &mbShareLaudanumEffect);
                HelpMarker("If a player drinks a Laudanum (health) potion, should BOTH\n"
                            "players receive the effect, or just the drinker themselves?");

                ImGui::Checkbox("Shared Oil Effect", &mbShareOilEffect);
                HelpMarker("Should both players get their lantern meter filled, or just\n"
                            "the one using it? Irrelevant if you share a single lantern,\n"
                            "since oil carries over to the other player when the lantern\n"
                            "is handed over. A player without a lantern can't use oil.");

                ImGui::Checkbox("Lantern Per Player", &mbLanternPerPlayer);
                HelpMarker("Should the lantern always be available for both players once\n"
                            "picked up, or must it be passed between players / found\n"
                            "separately by each?");

                ImGui::Checkbox("Shared Tinderboxes Count", &mbShareTinderboxes);
                HelpMarker("One tinderbox pool for both players: whoever picks one up\n"
                            "adds to the same count, and lighting a candle spends from it\n"
                            "whichever player struck it.\n\n"
                            "Off, each player carries their own tinderboxes exactly as\n"
                            "single player does.");
            }
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // =============================================
        //  COOP OPTIONS  (native co-op stories only -- script-owned, shown live)
        // =============================================
        if (GetForcedCoopState() == 2)
        {
            ImGui::TextColored(colSectionText, "COOP OPTIONS");
            HelpMarker("Gameplay options a co-op story defines from its own script,\n"
                        "at any time (OnGameStart, a map's OnEnter, mid-play):\n\n"
                        "  SetCoopAllowCrouchBoost(bool)\n"
                        "  SetCoopAllowPlayerCollision(bool)\n"
                        "  SetCoopAllowMonsterKilling(bool)\n"
                        "  SetCoopMonsterPropDamageMul(float)\n"
                        "  SetCoopGlobalLantern(bool)\n\n"
                        "This section is LIVE: it reads the story's current values\n"
                        "every frame, and edits here write straight back -- the same\n"
                        "state the script sees with the Get* variants. Saved with\n"
                        "the game. Only shown in a native co-op story; forced coop\n"
                        "uses the ordinary debug toggles instead.");
            ImGui::Separator();
            ImGui::Spacing();

            bool bBoost = GetNativeCoopOption(eImGuiCoopOption_CrouchBoost) != 0.0f;
            if (ImGui::Checkbox("Allow Crouched Head Boosting", &bBoost))
                SetNativeCoopOption(eImGuiCoopOption_CrouchBoost, bBoost ? 1.0f : 0.0f);
            HelpMarker("Landing on a crouched partner's head from above gives real,\n"
                        "standable, jump-off-able ground -- stand up under your\n"
                        "partner to boost them, Counter-Strike style. Side contact\n"
                        "and walking into each other stay free.");

            bool bColl = GetNativeCoopOption(eImGuiCoopOption_PlayerCollision) != 0.0f;
            if (ImGui::Checkbox("Allow Player Collision", &bColl))
                SetNativeCoopOption(eImGuiCoopOption_PlayerCollision, bColl ? 1.0f : 0.0f);
            HelpMarker("Full player-vs-player collision at all times, not just from\n"
                        "above. Might be hard to squeeze past each other, but some\n"
                        "stories prefer it. Implies head-standing.");

            bool bKill = GetNativeCoopOption(eImGuiCoopOption_MonsterKilling) != 0.0f;
            if (ImGui::Checkbox("Allow Monster Killing", &bKill))
                SetNativeCoopOption(eImGuiCoopOption_MonsterKilling, bKill ? 1.0f : 0.0f);
            HelpMarker("Thrown props damage monsters: mass in kg times the\n"
                        "multiplier below. Non-fatal hits make them recoil (flinch);\n"
                        "at zero health they collapse into a persistent ragdoll and\n"
                        "stop hunting, draining sanity and driving the music.");

            float fMul = GetNativeCoopOption(eImGuiCoopOption_MonsterDamageMul);
            if (ImGui::SliderFloat("Monster Damage Multiplier", &fMul, 0.5f, 100.0f, "%.1f dmg / kg"))
                SetNativeCoopOption(eImGuiCoopOption_MonsterDamageMul, fMul);
            HelpMarker("Damage per kilogram of thrown prop. Monsters have 100\n"
                        "health by default, so at 10 a 10 kg crate is a kill.");

            bool bLant = GetNativeCoopOption(eImGuiCoopOption_GlobalLantern) != 0.0f;
            if (ImGui::Checkbox("Global Lantern", &bLant))
                SetNativeCoopOption(eImGuiCoopOption_GlobalLantern, bLant ? 1.0f : 0.0f);
            HelpMarker("On: one found lantern serves both players -- either may\n"
                        "toggle it at any time. Off: a lantern belongs to whoever\n"
                        "carries it; find one each, or hand it over (double-click it\n"
                        "in the inventory to hold it out; giving it snuffs the\n"
                        "flame). A player holding two lanterns can pass one on.");

            ImGui::Spacing();
            ImGui::Spacing();
        }

        // =============================================
        //  AVATAR
        // =============================================
        ImGui::TextColored(colSectionText, "AVATAR");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("Debug View", &mbDebugView);
        HelpMarker("The level editor's view of the map: script areas and the\n"
                    "other invisible volumes drawn as labelled boxes.\n\n"
                    "Also what lets the console's `delete` remove an area --\n"
                    "you should not be able to delete what you cannot see.");

        ImGui::Checkbox("Show Colliders", &mbShowColliders);
        HelpMarker("Every physics shape in the world as wireframe, the way the\n"
                    "model editor draws them. Both players are left out -- you\n"
                    "are stood inside your own and it fills the screen.");

        ImGui::Checkbox("Show Water Lurkers", &mbShowWaterLurkers);
        HelpMarker("Water lurkers have a real model -- you can see it in the\n"
                    "level editor -- but the game hides it on load and again\n"
                    "every time the enemy activates, so all you ever get is the\n"
                    "wake on the surface.\n\n"
                    "This stops hiding it. Takes effect on the spot, on lurkers\n"
                    "already in the map.");

        ImGui::Spacing();

        ImGui::Checkbox("Invert Body Tilt", &mbCoopInvertBodyTilt);
        HelpMarker("Should the body lean backwards when going forward,\n"
                    "or forward when going forward? Lean left when going\n"
                    "left, or lean right when going right?");

        ImGui::Checkbox("Show Co-Op Legs", &mbCoopShowLegs);
        HelpMarker("Want leggies? This will show animated legs.");

        ImGui::Checkbox("Show Co-Op Arms", &mbCoopShowArms);
        HelpMarker("This will show shoulders and arms - but beware,\n"
                    "they do not line up with the lantern!");

        ImGui::Checkbox("Player 2 Wig", &mbCoopP2Wig);
        HelpMarker("Black shoulder-length hair on Player 2's avatar.\n\n"
                    "Player 2 is Justine, and a bare stickman head reads as\n"
                    "nobody in particular. Player 1 never gets one.");
        {
            // Must stay in step with eAvatarModel in LuxPlayerAvatar.h.
            static const char *vModels[] = { "Stickman", "Servant Grunt", "Servant Brute",
                                             "Agrippa", "Alexander", "Ritual Prisoner" };
            const int lModelNum = (int)(sizeof(vModels)/sizeof(vModels[0]));

            ImGui::Spacing();
            ImGui::Checkbox("Allow Player Model Change (Experimental)", &mbAllowCoopModelChange);
            HelpMarker("Off (the default) both players are the stickman and the two\n"
                        "combos below do nothing. The rigged models are experimental --\n"
                        "lantern holding and grabbing animations are broken on them.");

            if(mbAllowCoopModelChange==false) ImGui::BeginDisabled();
            ImGui::Combo("P1 Model", &mlCoopModelP1, vModels, lModelNum);
            ImGui::Combo("P2 Model", &mlCoopModelP2, vModels, lModelNum);
            ImGui::SliderFloat("Model Yaw Trim", &mfCoopModelYawTrim, -180.0f, 180.0f, "%.0f deg");
            if(mbAllowCoopModelChange==false) ImGui::EndDisabled();
            HelpMarker("The body each player is drawn as, in the OTHER player's\n"
                        "view. Changes apply immediately.\n\n"
                        "The character models are the game's own rigs, but their\n"
                        "OWN animations are not used -- the same solver that drives\n"
                        "the stickman (gait, foot planting, two-bone IK) drives\n"
                        "their bones instead. So they walk, crouch, lean and reach\n"
                        "exactly like the stickman does, with a body around it.\n\n"
                        "Because the bones are driven to the solver's joint\n"
                        "positions, every model comes out player-sized -- a brute\n"
                        "is human height, not hulking. Placement is right; the mesh\n"
                        "stretches to fit.\n\n"
                        "The show-legs and show-arms toggles above only apply to\n"
                        "the stickman. A rigged model is drawn whole.\n\n"
                        "Naked Guy (corpse_male) is deliberately absent: its rig has\n"
                        "no humanoid bones at all, just polySurface6 and joint26-50,\n"
                        "so there is nothing to map a skeleton onto.");
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // =============================================
        //  INTERACTION
        // =============================================
        ImGui::TextColored(colSectionText, "INTERACTION");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("Player Teleport", &mbPlayerTeleport);
        HelpMarker("Teleport players to each other:\n"
                    "F1 teleports P1 to P2, F2 teleports P2 to P1.\n"
                    "P2 can press down both joysticks to teleport to P1.");

        ImGui::Checkbox("Allow Grabbing Same Object", &mbAllowGrabSameObject);
        HelpMarker("Let both players grab the same physics object.\n"
                    "Can cause competing forces and erratic physics when\n"
                    "both pull at once. (On by default.)");

        ImGui::Spacing();
        ImGui::Spacing();

        // =============================================
        //  SCRIPT & CHEATS
        // =============================================
        ImGui::TextColored(colSectionText, "SCRIPT & CHEATS");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("Allow Cheats", &mbAllowCheats);
        HelpMarker("Enable cheat commands in the console.\n(Not yet implemented)");
        ImGui::SameLine();
        ImGui::TextColored(colNIText, " [NYI]");

        ImGui::Spacing();
        ImGui::Spacing();

        // =============================================
        //  RENDER DIAGNOSTICS  (read-only)
        // =============================================
        ImGui::TextColored(colSectionText, "RENDER DIAGNOSTICS");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Viewports drawn      : %d visible / %d total",
                    mlDiagVisibleViewports, mlDiagTotalViewports);
        HelpMarker("How many viewports cScene::Render actually draws.\n"
                    "Single player: 1.  Split-screen co-op: 2.\n\n"
                    "A 3 means the MAIN viewport is still visible underneath\n"
                    "the split views. It is created first, so it renders first\n"
                    "and the split views paint straight over it -- invisible on\n"
                    "screen, but a whole extra full-screen deferred pass every\n"
                    "single frame.");

        if(ImGui::Button("Reset Peaks")) ResetDiagProfilePeaks();
        HelpMarker("Per-module update cost. avg is the mean over the last second;\n"
                    "peak is the worst SINGLE frame since you last reset.\n\n"
                    "A huge peak next to a near-zero avg is ONE frame, not a\n"
                    "per-frame cost -- and possibly hours ago. Reset the peaks\n"
                    "and see whether it comes back before chasing it.\n\n"
                    "Lines starting ** are not modules: they are individual\n"
                    "blocking operations broken out of whichever module ran\n"
                    "them. ScreenShot, QuickSave and QuickLoad all run inside\n"
                    "cLuxInputHandler::Update, so without these the whole cost\n"
                    "of a game load reads as LuxInputHandler taking 3 seconds.\n\n"
                    "For the stutter that builds up: reset the peaks, play for a\n"
                    "few minutes without leaving the map, then look at which line\n"
                    "grew. That names the subsystem. Note the whole render pass is\n"
                    "one module (LuxMapHandler drives it), so if rendering is the\n"
                    "cost it shows up there.");

        // Walk the PEAK table, not the average table. LatchDiagFrameCounters
        // rebuilds mMapDiagModuleAvg from scratch every 60 frames, so anything not
        // hit in the last second disappeared from this list entirely -- taking its
        // remembered peak with it. That is exactly backwards: a one-off multi-second
        // stall is the whole reason the peak column exists, and it was the one thing
        // guaranteed to scroll away before it could be read. Peaks persist until
        // Reset Peaks, so drive the list off them and look the average up.
        for(std::map<std::string, float>::iterator peakIt = mMapDiagModulePeak.begin();
            peakIt != mMapDiagModulePeak.end(); ++peakIt)
        {
            float fAvg = 0;
            std::map<std::string, float>::iterator avgIt = mMapDiagModuleAvg.find(peakIt->first);
            if(avgIt != mMapDiagModuleAvg.end()) fAvg = avgIt->second;

            if(fAvg < 0.02f && peakIt->second < 0.5f) continue;

            ImGui::Text("  %-22s avg %6.2f ms   peak %6.2f ms",
                        peakIt->first.c_str(), fAvg, peakIt->second);
        }

        ImGui::Spacing();

        ImGui::Text("Lights rendered      : %d", mlDiagLights);
        HelpMarker("Lights lit across all viewports this frame. THIS is the number\n"
                    "to watch for the stutter that builds up: if it climbs the\n"
                    "longer you stand in one map, the per-frame work really is\n"
                    "growing. If it is flat while the stutter worsens, the cost is\n"
                    "somewhere else and the growth is not in the renderer.");

        ImGui::Text("Occlusion queries    : %d", mlDiagQueries);
        HelpMarker("Occlusion queries issued across all viewports this frame.\n"
                    "Same idea as the light count -- watch whether it grows.");

        ImGui::Text("Shadow maps / frame  : %d", mlDiagShadowMapRenders);
        HelpMarker("iRenderer::RenderShadowMap calls this frame.\n\n"
                    "The shadow cache lives on the RENDERER, not the viewport,\n"
                    "and the frame counter ticks once per cScene::Render rather\n"
                    "than once per viewport -- so each extra viewport evicts\n"
                    "shadow maps the previous one just rendered.\n\n"
                    "Compare co-op on vs off, and watch whether it climbs the\n"
                    "longer you stay in one map.");

        ImGui::Spacing();

        ImGui::TextWrapped("P1 avatar: %s",
                    msDiagAvatar[0].empty() ? "(no report)" : msDiagAvatar[0].c_str());
        ImGui::TextWrapped("P2 avatar: %s",
                    msDiagAvatar[1].empty() ? "(no report)" : msDiagAvatar[1].c_str());
        HelpMarker("Pushed every frame by cLuxPlayerAvatar::SetEntitiesVisible,\n"
                    "which is the ONE place that decides whether the other\n"
                    "player is drawn. Read it when someone goes invisible --\n"
                    "each way that can happen reads differently:\n\n"
                    "  (no report)     SetEntitiesVisible is not being called\n"
                    "                  at all, so the pre-draw callback never\n"
                    "                  reaches this player.\n"
                    "  world DEAD/NULL the cached world is gone; the parts are\n"
                    "                  stale and nothing may touch them.\n"
                    "  world != mapworld\n"
                    "                  parts belong to a world that is no longer\n"
                    "                  the current map.\n"
                    "  parts 8/9       CreateWorldEntities never finished.\n"
                    "  body NULL       no character body -- dead, or mid-load.\n"
                    "  vis 0           everything is fine and the avatar is\n"
                    "                  being hidden ON PURPOSE for this view.\n\n"
                    "Both lines update from BOTH viewports, so the value you\n"
                    "see is whichever viewport drew last.");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Close button — centered
        float buttonW = 90.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonW) * 0.5f);
        if (ImGui::Button("Close", ImVec2(buttonW, 0)))
        {
            Toggle();
        }
        ImGui::Spacing();

        ImGui::EndGroup();

        // ---- RIGHT COLUMN ---------------------------------------------
        // Everything new goes here. The left column is already taller than most
        // screens; adding to the bottom of it makes things unfindable.
        ImGui::SameLine(0.0f, 28.0f);
        ImGui::BeginGroup();

        // =============================================
        //  MONSTER MANIPULATION
        // =============================================
        ImGui::TextColored(colSectionText, "MONSTER MANIPULATION");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("Allow Killing Monsters", &mbAllowKillingMonsters);
        HelpMarker("Not so defenceless now, are we?\n\n"
                    "Throw physics props at a monster to hurt it. At zero health it\n"
                    "collapses into a ragdoll that stays in the world and still\n"
                    "collides with everything, and it stops draining sanity, stops\n"
                    "the heartbeat, and drops out of the hunt and chase music.\n\n"
                    "OFF is vanilla: props do no damage and monsters cannot die.");

        ImGui::SliderFloat("Prop Physics Damage Multiplier", &mfPropDamagePerKg,
                            0.5f, 100.0f, "%.1f dmg / kg");
        HelpMarker("Damage a thrown prop deals = its MASS in kilograms times this.\n\n"
                    "Monsters have 100 health by default, so at 10 a 10 kg crate is\n"
                    "an instant kill and a 1 kg mug takes ten hits.\n\n"
                    "A prop has to actually be moving to count, and the same monster\n"
                    "cannot be hit again for a moment afterwards -- otherwise a\n"
                    "barrel rolling against it would register a hit every frame.");

        ImGui::Checkbox("1 Hit With Prop to Kill", &mbOneHitPropKill);
        HelpMarker("Any prop, any weight, one throw. Ignores the multiplier.\n\n"
                    "Good for testing the ragdoll without hunting for a heavy crate,\n"
                    "and for people with skill issues.");

        ImGui::Spacing();
        ImGui::Spacing();

        // =============================================
        //  GENERAL CHEATS
        // =============================================
        ImGui::TextColored(colSectionText, "GENERAL CHEATS");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("Free Use Candles", &mbFreeUseCandles);
        HelpMarker("Light candles, torches and lamps with no tinderbox at all,\n"
                    "and without spending the ones you are carrying.\n\n"
                    "Applies to whichever player strikes the light, so it covers\n"
                    "both of you from this one switch. The sanity you gain from\n"
                    "lighting a room is unchanged.");

        ImGui::Checkbox("Free Use Lantern", &mbFreeUseLantern);
        HelpMarker("The lantern burns no oil, and turns on at zero oil.\n\n"
                    "Both halves matter: without the second one an empty lantern\n"
                    "still refuses to light, and the cheat would look broken.\n\n"
                    "Applies to both players.");

        ImGui::Spacing();
        ImGui::Spacing();

        // =============================================
        //  DEBUG GUNS
        // =============================================
        ImGui::TextColored(colSectionText, "POSSESSION");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("Possess Enemies (H)", &mbAllowPossession);
        HelpMarker("Player 1 only. Look at a monster and press H to take it\n"
                    "over; press H again to give it back.\n\n"
                    "While possessed that monster stops driving the chase\n"
                    "music, the heartbeat and the sanity drain, and its AI\n"
                    "stops deciding for it -- but it is still the monster\n"
                    "moving. Steering goes through the same MoveToPos the\n"
                    "pathfinder uses, so the turn rate, the skid on a hard\n"
                    "turn and the walk/run cycle are all its own.\n\n"
                    "Mouse 1 swings, mouse 2 smashes a door in front of it,\n"
                    "Shift runs. Your own body stays where you left it.\n\n"
                    "Unticking this releases immediately -- it is the panic\n"
                    "button if anything gets stuck.");

        ImGui::SliderFloat("Possess Cam Distance", &mfPossessCamDistance, 0.0f, 8.0f, "%.2f");
        HelpMarker("How far the chase camera sits behind the monster. It is\n"
                    "pulled in automatically when a wall is in the way.\n\n"
                    "0 puts you inside its head.");

        ImGui::SliderFloat("Possess Cam Height", &mfPossessCamHeight, -1.0f, 3.0f, "%.2f");
        HelpMarker("Height of the point the camera looks at, measured from the\n"
                    "monster's body centre. Raise it for the taller ones.\n\n"
                    "Shared with Enemy Morph below.");

        ImGui::Spacing();
        ImGui::TextColored(colSectionText, "BLACK FRAME CATCHER");
        ImGui::Separator();
        ImGui::Spacing();

        {
            const int lCount = GetDiagBlackViewportCount();

            if(lCount == 0)
            {
                ImGui::TextColored(ImVec4(0.45f, 0.9f, 0.45f, 1.0f),
                                   "No black viewport frames since reset.");
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f),
                                   "BLACK FRAMES: %d   (last on render frame %d)",
                                   lCount, GetDiagBlackViewportLastFrame());
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.45f, 1.0f),
                                   "  %s", GetDiagBlackViewportText());
            }

            HelpMarker("Counts frames where a VISIBLE viewport drew no world.\n\n"
                        "That is a black half-screen: the frame was cleared for it,\n"
                        "the GUI still drew on top, and nothing else did -- which is\n"
                        "what the flicker looks like.\n\n"
                        "The line underneath names which viewport, and which of\n"
                        "renderer / world / camera / frustum came back NULL. All four\n"
                        "look the same on screen and want different fixes, so the\n"
                        "number alone is not enough.\n\n"
                        "If this stays at zero while the screen still flickers, the\n"
                        "frame IS being drawn and the fault is later -- the swap, the\n"
                        "encoder, or the display.");

            if(ImGui::Button("Reset black frame counter")) ResetDiagBlackViewport();
        }

        ImGui::Spacing();
        ImGui::TextColored(colSectionText, "CO-OP STATE");
        ImGui::Separator();
        ImGui::Spacing();

        {
            const std::string sDiag = GetCoopDiag();
            ImGui::TextUnformatted(sDiag.c_str());
            HelpMarker("Everything the co-op menu paths actually decide from, as they\n"
                        "see it right now. Open a journal or pick up a note and read it\n"
                        "off: it says which container is live, who owns the menu, where\n"
                        "its viewport is, and which device handles are paired.");
        }

        ImGui::Spacing();
        ImGui::TextColored(colSectionText, "INPUT DEVICES");
        ImGui::Separator();
        ImGui::Spacing();

        {
            hpl::cRawInputWin32 *pRaw = hpl::cRawInputWin32::GetInstance();

            if(pRaw == NULL || pRaw->IsAvailable() == false)
            {
                ImGui::TextDisabled("Raw input unavailable (Windows only).");
                HelpMarker("SDL has one system keyboard and one system cursor, so two\n"
                            "keyboards look like one to it. Windows raw input tags every\n"
                            "event with the device that sent it, which is what this reads.");
            }
            else
            {
                ImGui::Text("Devices seen: %d", (int)pRaw->GetDevices().size());
                HelpMarker("Type or move a device to make it appear and count up.\n\n"
                            "INJECTED (SendInput) is the important row: that is what a\n"
                            "Moonlight or Parsec guest arrives as, because injected input\n"
                            "has no hardware behind it and so carries no device handle.\n"
                            "It is what lets a streamed second player be told apart from\n"
                            "the person sitting at the machine, with nothing to pair.\n\n"
                            "Anything else on the host that drives SendInput -- macro\n"
                            "tools, the on-screen keyboard, Steam's controller-as-keyboard\n"
                            "emulation -- lands in that same row.");

                ImGui::Spacing();

                const std::map<hpl::tRawInputDevice, hpl::cRawInputDeviceState> &mapDevices = pRaw->GetDevices();
                std::map<hpl::tRawInputDevice, hpl::cRawInputDeviceState>::const_iterator it = mapDevices.begin();

                for(; it != mapDevices.end(); ++it)
                {
                    const hpl::cRawInputDeviceState &state = it->second;

                    char sKind[32];
                    snprintf(sKind, sizeof(sKind), "%s%s",
                             state.mbIsKeyboard ? "keyboard" : "",
                             state.mbIsMouse ? (state.mbIsKeyboard ? "+mouse" : "mouse") : "");

                    //Green marks the injected row so it is findable at a glance.
                    const bool bInjected = (it->first == kRawInputInjectedDevice);
                    const ImVec4 col = bInjected ? ImVec4(0.45f, 0.9f, 0.45f, 1.0f)
                                                 : ImVec4(0.85f, 0.85f, 0.85f, 1.0f);

                    char sMotion[32];
                    sMotion[0] = 0;
                    if(state.mlRelEventCount || state.mlAbsEventCount)
                        snprintf(sMotion, sizeof(sMotion), "  rel %d / abs %d",
                                 state.mlRelEventCount, state.mlAbsEventCount);

                    if(bInjected)
                        ImGui::TextColored(col, "  INJECTED (SendInput)  %-14s events %d%s",
                                            sKind, state.mlEventCount, sMotion);
                    else
                        ImGui::TextColored(col, "  device %p  %-14s events %d%s",
                                            it->first, sKind, state.mlEventCount, sMotion);
                }

                if(mapDevices.empty())
                    ImGui::TextDisabled("  (press a key or move a mouse)");

                ImGui::Spacing();
                if(pRaw->GetLastActiveKeyboard() != kRawInputInjectedDevice)
                    ImGui::Text("Last keyboard used: %p", pRaw->GetLastActiveKeyboard());
                else
                    ImGui::Text("Last keyboard used: INJECTED");

                ImGui::Spacing();

                // ---- Player 2's input source ----
                const char *vSourceNames[] = { "Gamepad", "Second keyboard + mouse" };
                ImGui::Combo("Player 2 Input", &mlP2InputSource, vSourceNames, 2);
                HelpMarker("Gamepad is the original and the default.\n\n"
                            "Second keyboard + mouse gives Player 2 its own keyboard\n"
                            "and mouse instead, told apart by the device handle above.\n"
                            "Player 2 uses the SAME layout as Player 1 -- WASD, Shift,\n"
                            "Ctrl, Space, F, and the mouse -- just on their own device,\n"
                            "so there is nothing new to learn and nothing to rebind.\n\n"
                            "Windows only, and no help without co-op switched on.");

                if(mlP2InputSource == 1)
                {
                    // Whichever device is picked here is Player 2; everything else
                    // is Player 1, which is how the two get separated downstream.
                    //Sets BOTH. A streamed guest's keyboard and mouse arrive with the
                    //same absent device behind them, so picking one has always meant
                    //picking the other.
                    if(ImGui::RadioButton("Streamed guest (injected)",
                        mpP2RawDevice == kRawInputInjectedDevice && mpP2RawMouse == kRawInputInjectedDevice))
                    {
                        mpP2RawDevice = kRawInputInjectedDevice;
                        mpP2RawMouse  = kRawInputInjectedDevice;
                        mbP2RawMouseUserSet = true;
                    }
                    HelpMarker("For a friend playing over Moonlight or Parsec. Their\n"
                                "keyboard and mouse arrive injected, with no device\n"
                                "behind them, so they separate from yours by themselves\n"
                                "-- nothing to pair, nothing to pick.\n\n"
                                "Careful if anything else on this PC drives input the\n"
                                "same way: macro tools, the on-screen keyboard, Steam's\n"
                                "controller-as-keyboard emulation. Those would be\n"
                                "Player 2 as well.");

                    const std::map<hpl::tRawInputDevice, hpl::cRawInputDeviceState> &mapDev = pRaw->GetDevices();
                    std::map<hpl::tRawInputDevice, hpl::cRawInputDeviceState>::const_iterator devIt = mapDev.begin();

                    for(; devIt != mapDev.end(); ++devIt)
                    {
                        if(devIt->first == kRawInputInjectedDevice) continue;
                        if(devIt->second.mbIsKeyboard == false) continue;

                        char sLabel[64];
                        snprintf(sLabel, sizeof(sLabel), "Keyboard %p", devIt->first);

                        if(ImGui::RadioButton(sLabel, mpP2RawDevice == devIt->first))
                            mpP2RawDevice = devIt->first;
                    }

                    if(ImGui::Button("Use last keyboard pressed"))
                        mpP2RawDevice = pRaw->GetLastActiveKeyboard();
                    HelpMarker("For two keyboards plugged into this machine: have\n"
                                "Player 2 press a key, then click this.");

                    //////////////////////////////////////////////////////////////
                    // The MOUSE, separately.
                    //
                    // Raw input gives a keyboard and a mouse two different handles,
                    // so Player 2's keyboard says nothing about which mouse is
                    // theirs. Picking only the keyboard left every mouse question --
                    // motion, buttons, and the subtraction that keeps Player 2's aim
                    // out of Player 1's view -- answered against a device that has
                    // never sent a mouse event, which reads as "no mouse at all".
                    ImGui::Spacing();
                    ImGui::Text("Player 2 mouse");

                    {
                        const std::map<hpl::tRawInputDevice, hpl::cRawInputDeviceState> &mapMice = pRaw->GetDevices();
                        std::map<hpl::tRawInputDevice, hpl::cRawInputDeviceState>::const_iterator mouseIt = mapMice.begin();

                        for(; mouseIt != mapMice.end(); ++mouseIt)
                        {
                            if(mouseIt->first == kRawInputInjectedDevice) continue;
                            if(mouseIt->second.mbIsMouse == false) continue;

                            char sMouseLabel[64];
                            snprintf(sMouseLabel, sizeof(sMouseLabel), "Mouse %p", mouseIt->first);

                            if(ImGui::RadioButton(sMouseLabel, mpP2RawMouse == mouseIt->first))
                            {
                                mpP2RawMouse = mouseIt->first;
                                mbP2RawMouseUserSet = true;
                            }
                        }
                    }

                    if(ImGui::Button("Use last mouse moved"))
                    {
                        mpP2RawMouse = pRaw->GetLastActiveMouse();
                        mbP2RawMouseUserSet = true;
                    }
                    HelpMarker("For two mice plugged into this machine: have Player 2\n"
                                "move theirs, then click this.\n\n"
                                "Skip it for a streamed guest -- injected input has no\n"
                                "device behind it, so the button above already covers\n"
                                "their mouse as well as their keyboard.");

                    //////////////////////////////////////////////////////////////
                    // Say so when the mouse has not actually been picked.
                    //
                    // Getting this wrong is completely silent: the handle is still a
                    // valid handle, every mouse question asked of it simply answers
                    // "no motion, no buttons", and that is indistinguishable from the
                    // whole feature not working. It cost three rounds of chasing the
                    // wrong thing, so it is worth a line of red.
                    {
                        const std::map<hpl::tRawInputDevice, hpl::cRawInputDeviceState> &mapChk = pRaw->GetDevices();
                        std::map<hpl::tRawInputDevice, hpl::cRawInputDeviceState>::const_iterator chkIt =
                            mapChk.find((hpl::tRawInputDevice)mpP2RawMouse);

                        const bool bMouseSeen = (chkIt != mapChk.end() && chkIt->second.mbIsMouse);

                        if(bMouseSeen == false)
                        {
                            if(mpP2RawDevice == kRawInputInjectedDevice)
                            {
                                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f),
                                    "Waiting for Player 2 to move their mouse.");
                            }
                            else
                            {
                                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f),
                                    "Pairing automatically -- have Player 2 walk and look.");
                                HelpMarker("A keyboard and a mouse are separate devices with\n"
                                            "separate handles, so picking Player 2's keyboard\n"
                                            "says nothing about which mouse is theirs.\n\n"
                                            "It works this out on its own: the mouse that keeps\n"
                                            "moving while Player 2's movement keys are held is\n"
                                            "Player 2's. A few seconds of them walking around is\n"
                                            "enough. Picking one here by hand overrides it for\n"
                                            "good.");
                            }
                        }
                    }
                }
            }
        }

        ImGui::Spacing();
        ImGui::TextColored(colSectionText, "ENEMY MORPH");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("Enable Morphing (1-4)", &mbAllowEnemyMorph);
        HelpMarker("Player 1 only. Become a monster without needing one to\n"
                    "possess: the monster is spawned where you stand and handed\n"
                    "straight to you, and your own body is put away until you\n"
                    "change back -- Player 2 sees the monster, not you.\n\n"
                    "Keys 1, 2, 3 and 4 are Grunt, Brute, Suitor and Water\n"
                    "Lurker. The same key again changes you back; a different\n"
                    "one swaps you straight over. You come back standing where\n"
                    "the monster was.\n\n"
                    "Controls are possession's: move and look as normal, Shift\n"
                    "runs, mouse 1 swings, mouse 2 smashes a door in front of\n"
                    "you. The camera sliders above apply here too.\n\n"
                    "A morphed monster is never written to a save, and you are\n"
                    "changed back on a map change. Unticking this changes you\n"
                    "back immediately -- the same panic button possession has.");

        //Disabled rather than hidden, so the buttons stay where the eye learned
        //them and the reason they do nothing is on the tin.
        if(mbAllowEnemyMorph==false) ImGui::BeginDisabled();

        if(ImGui::Button("Become Grunt"))        RequestEnemyMorph(0);
        ImGui::SameLine();
        if(ImGui::Button("Become Brute"))        RequestEnemyMorph(1);

        if(ImGui::Button("Become Suitor"))       RequestEnemyMorph(2);
        ImGui::SameLine();
        if(ImGui::Button("Become Water Lurker")) RequestEnemyMorph(3);

        if(mbAllowEnemyMorph==false) ImGui::EndDisabled();

        HelpMarker("The Suitor lives in the Justine content (entities/ptest); if\n"
                    "that is not installed the morph is refused and says so.\n\n"
                    "A Water Lurker has no model to show unless Show Water\n"
                    "Lurkers is on, and it only swims -- on dry land you get an\n"
                    "invisible monster that will not move.");

        ImGui::Spacing();
        ImGui::TextColored(colSectionText, "DEBUG GUNS");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("Debug Guns", &mbDebugGuns);
        HelpMarker("G raises Player 1's gun, LB raises Player 2's. Fire with the\n"
                    "attack button -- right mouse, or the right trigger on the pad.\n\n"
                    "Hitscan: no ammo, no reload, just a short cooldown. Tracers are\n"
                    "your player colour, the muzzle flash is a real world light both\n"
                    "of you can see, and the impact plays whatever the surface it hit\n"
                    "is supposed to play.\n\n"
                    "It carries like the lantern, so only one of the two is up at a\n"
                    "time -- raising the gun stows the lantern and vice versa.");

        ImGui::SliderFloat("Gun Impact Force", &mfGunImpactForce, 0.0f, 500.0f, "%.0f");
        HelpMarker("How hard a shot shoves what it hits.\n\n"
                    "Props: divided by mass, so a chair flies and a wardrobe\n"
                    "barely notices.\n\n"
                    "Monsters: added to their velocity at a tenth of this. They\n"
                    "walk at about 2, so the default of 5 is a small nudge rather\n"
                    "than a shove. Raise it if you want them staggered.");

        ImGui::SliderFloat("Gun Damage", &mfGunDamage, 1.0f, 200.0f, "%.0f");
        HelpMarker("Per shot. Monsters have 100 health, so the default of 25 is\n"
                    "four shots to put one down; 100 is a one-shot kill.\n\n"
                    "Gun damage ignores the resistance every enemy gets while it\n"
                    "is hunting you, so four shots means four shots whatever the\n"
                    "monster happens to be doing. Non-fatal hits stagger it and\n"
                    "play its flinch.\n\n"
                    "Killing one at all needs Allow Killing Monsters on, since\n"
                    "that is what turns the ragdoll and the shutdown on.");

        ImGui::EndGroup();
    }

    // Handle X button closing the window
    if (!mbVisible)
    {
        SDL_ShowCursor(SDL_DISABLE);
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }

    ImGui::End();

    ImGui::PopStyleColor(14);
    ImGui::PopStyleVar(5);
}
