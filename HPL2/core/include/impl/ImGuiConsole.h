#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

struct ImGuiInputTextCallbackData;

class cImGuiConsole
{
public:
    typedef std::vector<std::string> tArgs;
    typedef std::function<void(const tArgs&)> tCommandFn;

    // Given what the user has typed so far for the NEXT argument, return every
    // value it could become. Empty prefix means "list everything".
    typedef std::function<std::vector<std::string>(const std::string&)> tCompleteFn;

    // Names are matched case-INSENSITIVELY -- the map is keyed on the lowercased
    // name -- but msDisplayName keeps the spelling it was registered with, so
    // the type-ahead can show `undoArray` while `undoarray` still works. Purely
    // a readability thing: a run-on lowercase word is hard to read at a glance.
    struct sCommand
    {
        std::string msDisplayName;
        std::string msHelp;
        tCommandFn  mFn;
        tCompleteFn mCompleteFn;
    };

    // ------------------------------------------------------------------
    // VARIABLES
    //
    // A variable is a named value rather than an action: typing its name alone
    // prints what it currently is, typing "name value" changes it. The
    // autocomplete list shows the live value beside every variable it offers,
    // which is the whole reason they are a separate kind -- a command has
    // nothing to show there.
    typedef std::function<std::string()> tGetValueFn;
    typedef std::function<void(const std::string&)> tSetValueFn;

    struct sVariable
    {
        std::string msDisplayName;
        std::string msHelp;
        tGetValueFn mGetFn;
        tSetValueFn mSetFn;     //empty = read only
    };

    static void Init();
    static void Shutdown();
    static void Draw();
    static void Toggle();      // Toggle the whole console bar on/off
    static void SetVisible(bool abX); // Force a state (options menu turning it off)
    static void ToggleLog();   // Toggle the log panel (Shift + §)
    static bool IsVisible();
    static bool IsLogExpanded();

    static void SetCommandCallback(std::function<void(const std::string&)> aCallback);
    static void RegisterCommand(const std::string& aName, const std::string& aHelp, tCommandFn aFn);
    /**
     * Teach Tab what this command's arguments can be. Optional; a command with
     * no provider simply does not complete past its own name.
     */
    static void RegisterCompletion(const std::string& aName, tCompleteFn aFn);
    static void UnregisterCommand(const std::string& aName);

    /** Register a named value. A null aSetFn makes it read-only. */
    static void RegisterVariable(const std::string& aName, const std::string& aHelp,
                                 tGetValueFn aGetFn, tSetValueFn aSetFn = nullptr);
    static void UnregisterVariable(const std::string& aName);

    /** Current value of a registered variable; false if there is no such name. */
    static bool GetVariableValue(const std::string& aName, std::string& asOut);
    static void AddLog(const char* fmt, ...);
    static void ClearLog();

private:
    static bool mbVisible;
    static bool mbLogExpanded;   // <-- NEW: log panel shown only when toggled
    static bool mbReclaimFocus;
    static char msInputBuf[512];

    static std::vector<std::string> mvLog;
    static std::vector<std::string> mvHistory;
    static int mlHistoryPos;

    static std::function<void(const std::string&)> mCommandCallback;
    static std::unordered_map<std::string, sCommand> mCommands;
    static std::unordered_map<std::string, sVariable> mVariables;

    // ------------------------------------------------------------------
    // Live autocomplete.
    //
    // mvSuggestions is rebuilt every frame from the word being typed and drawn
    // as a drop-down under the input bar. mlSuggestionTotal is the number of
    // matches BEFORE capping, so the panel can say how many it is not showing.
    //
    // Tab writes its completion straight into the buffer -- it is real text you
    // can carry on typing onto. msGhostText is the part Tab added and
    // mlGhostStart where it begins, so it can be redrawn in green ON TOP of
    // what InputText already painted. The moment the line stops ending in
    // exactly that text the colour is dropped and the text simply stays,
    // which is the whole behaviour.
    //
    // msGhostStem is what was typed before Tab ran, kept so pressing Tab again
    // can walk to the next match instead of re-completing its own output.
    static std::vector<std::string> mvSuggestions;
    static int mlSuggestionTotal;
    static std::string msGhostText;
    static std::string msGhostStem;
    static int mlGhostStart;
    static int mlGhostMatch;


    /** True while the buffer still ends in the exact text Tab added. */
    static bool GhostIsFresh();

    /** Every command and variable name starting with the word being typed. */
    static void CollectMatches(const std::string& asWord, std::vector<std::string>& avOut);
    static void RefreshSuggestions();
    static bool IsVariable(const std::string& asName);

    static std::string Trim(const std::string& s);
    static std::string ToLower(const std::string& s);
    static tArgs Tokenize(const std::string& s);

    static void ExecLine(const std::string& aLine);
    static bool TryExecBuiltin(const std::string& aLine);
    static void RegisterBuiltinCommands();

    static int TextEditCallback(ImGuiInputTextCallbackData* data);
    static void HandleCompletion(ImGuiInputTextCallbackData* data);
};
