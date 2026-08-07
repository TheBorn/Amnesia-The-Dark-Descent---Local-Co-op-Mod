#pragma once

struct SDL_Window;
typedef void* SDL_GLContext;
union SDL_Event;


class ImGuiManager
{
public:
    static void Init(SDL_Window* apWindow, SDL_GLContext aGLContext);
    static void Shutdown();

    // Call for each SDL event inside the poll loop
    static void ProcessEvent(SDL_Event* apEvent);

    // Call once per frame before your own ImGui drawing code
    static void NewFrame();

    // Call once per frame after your own ImGui drawing code, before SwapBuffers
    static void Render();

    static bool IsInitialized();

    // What the last Render() actually submitted. Zero means ImGui drew nothing that
    // frame, which is what tells a black frame caused by the game apart from one
    // caused by an ImGui overlay.
    static void GetLastDrawStats(int& alCmdLists, int& alVerts);

    // Toggle the demo window (F1 by default)
    static void ToggleDemo();

    // ------------------------------------------------------------------
    // Player-facing gates for the debug overlays.
    //
    // The console (grave/tilde), the debug menu (Insert) and the frame-trace
    // dump (F9) are developer tools, not features, so they answer their hotkey
    // only when the player has switched them on in Options -> Debug. Both start
    // OFF; cLuxConfigHandler pushes the saved choice in at startup.
    //
    // Switching one off also hides it, so a player cannot be left staring at an
    // overlay whose hotkey no longer works.
    static void SetDebugMenuEnabled(bool abX);
    static bool GetDebugMenuEnabled();
    static void SetConsoleEnabled(bool abX);
    static bool GetConsoleEnabled();

private:
    static bool mbInitialized;
    static bool mbShowDemo;
    static bool mbDebugMenuEnabled;
    static bool mbConsoleEnabled;
    static int mlLastCmdLists;
    static int mlLastVerts;
};