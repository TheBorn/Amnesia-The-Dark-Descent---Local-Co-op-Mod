#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "SDL2/SDL.h"
#include <GL/glew.h>
#include <impl/ImGuiManager.h>
#include "impl/ImGuiConsole.h"
#include "impl/ImGuiDebugMenu.h"

bool ImGuiManager::mbInitialized = false;
bool ImGuiManager::mbShowDemo = false;
bool ImGuiManager::mbDebugMenuEnabled = false;
bool ImGuiManager::mbConsoleEnabled = false;
int ImGuiManager::mlLastCmdLists = 0;
int ImGuiManager::mlLastVerts = 0;

void ImGuiManager::Init(SDL_Window* apWindow, SDL_GLContext aGLContext)
{
    if (mbInitialized) return;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;  // Never let ImGui touch the OS cursor
    // Optionally: io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    // Use GLSL 1.20 to match Amnesia's legacy GL context
    ImGui_ImplSDL2_InitForOpenGL(apWindow, aGLContext);
    ImGui_ImplOpenGL3_Init("#version 120");

    cImGuiConsole::Init();
    ImGuiDebugMenu::Init();

    mbInitialized = true;
}

void ImGuiManager::Shutdown()
{
    if (!mbInitialized) return;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    cImGuiConsole::Shutdown();
    ImGuiDebugMenu::Shutdown();

    mbInitialized = false;
}

//Flight recorder dump request -- defined in cLowLevelGraphicsSDL.cpp.
namespace hpl { extern bool gLuxFrameTraceDumpRequested; }

void ImGuiManager::SetDebugMenuEnabled(bool abX)
{
    mbDebugMenuEnabled = abX;
    if (abX == false) ImGuiDebugMenu::SetVisible(false);
}

bool ImGuiManager::GetDebugMenuEnabled()
{
    return mbDebugMenuEnabled;
}

void ImGuiManager::SetConsoleEnabled(bool abX)
{
    mbConsoleEnabled = abX;
    if (abX == false) cImGuiConsole::SetVisible(false);
}

bool ImGuiManager::GetConsoleEnabled()
{
    return mbConsoleEnabled;
}

void ImGuiManager::ProcessEvent(SDL_Event* apEvent)
{
    if (!mbInitialized) return;

    if (mbDebugMenuEnabled &&
        apEvent->type == SDL_KEYDOWN && apEvent->key.keysym.scancode == SDL_SCANCODE_INSERT)
    {
        ImGuiDebugMenu::Toggle();
        return;
    }

    // F9: dump the flight recorder -- the last ~20 seconds of per-frame render
    // evidence -- as CSV next to hpl.log. Press it right after a flicker.
    // Part of the debug menu's toolset, so it follows the same gate.
    if (mbDebugMenuEnabled &&
        apEvent->type == SDL_KEYDOWN && apEvent->key.keysym.scancode == SDL_SCANCODE_F9)
    {
        hpl::gLuxFrameTraceDumpRequested = true;
        return;
    }

    if (mbConsoleEnabled &&
        apEvent->type == SDL_KEYDOWN && apEvent->key.keysym.scancode == SDL_SCANCODE_GRAVE)  
    {  
        if (apEvent->key.keysym.mod & KMOD_SHIFT)
        {
            // Shift+§ = toggle log panel (BO3-style full console)
            cImGuiConsole::ToggleLog();
        }
        else
        {
            // § alone = toggle console bar
            cImGuiConsole::Toggle();
        }
        return;  
    }  
  
    // When console or debug menu is open, eat all input so game doesn't react  
    if (cImGuiConsole::IsVisible() || ImGuiDebugMenu::IsVisible())  
    {  
        ImGui_ImplSDL2_ProcessEvent(apEvent);  
        return;  
    }
    
    ImGui_ImplSDL2_ProcessEvent(apEvent);
}

void ImGuiManager::NewFrame()
{
    if (!mbInitialized) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    cImGuiConsole::Draw();
    ImGuiDebugMenu::Draw();
}

void ImGuiManager::Render()
{
    if (!mbInitialized) return;

    // --- Your debug UI goes here ---
    if (mbShowDemo)
    {
        ImGui::ShowDemoWindow(&mbShowDemo);
    }
    // ----

    ImGui::Render();

    ImDrawData* pDrawData = ImGui::GetDrawData();
    mlLastCmdLists = pDrawData ? pDrawData->CmdListsCount : 0;
    mlLastVerts    = pDrawData ? pDrawData->TotalVtxCount : 0;

    ImGui_ImplOpenGL3_RenderDrawData(pDrawData);
}

void ImGuiManager::GetLastDrawStats(int& alCmdLists, int& alVerts)
{
    alCmdLists = mlLastCmdLists;
    alVerts    = mlLastVerts;
}

bool ImGuiManager::IsInitialized()
{
    return mbInitialized;
}

void ImGuiManager::ToggleDemo()
{
    mbShowDemo = !mbShowDemo;
}