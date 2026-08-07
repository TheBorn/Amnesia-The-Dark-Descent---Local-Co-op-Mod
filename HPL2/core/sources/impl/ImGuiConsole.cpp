#include "impl/ImGuiConsole.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "imgui.h"

// ----
// Static members
// ----
bool cImGuiConsole::mbVisible = false;
bool cImGuiConsole::mbLogExpanded = false;
bool cImGuiConsole::mbReclaimFocus = false;

char cImGuiConsole::msInputBuf[512] = {};

std::vector<std::string> cImGuiConsole::mvLog;
std::vector<std::string> cImGuiConsole::mvHistory;
int cImGuiConsole::mlHistoryPos = -1;

std::function<void(const std::string&)> cImGuiConsole::mCommandCallback = nullptr;
std::unordered_map<std::string, cImGuiConsole::sCommand> cImGuiConsole::mCommands;
std::unordered_map<std::string, cImGuiConsole::sVariable> cImGuiConsole::mVariables;

std::vector<std::string> cImGuiConsole::mvSuggestions;
int cImGuiConsole::mlSuggestionTotal = 0;
std::string cImGuiConsole::msGhostText;
std::string cImGuiConsole::msGhostStem;
int cImGuiConsole::mlGhostStart = 0;
int cImGuiConsole::mlGhostMatch = 0;

// How many suggestion rows the drop-down will draw before it gives up and just
// says how many there were. Past a dozen or so the list stops being something
// you read and starts being something you scroll, which is not what a
// type-ahead is for.
static const int kMaxSuggestionRows = 12;

// ----
// Small string helpers
// ----
std::string cImGuiConsole::Trim(const std::string& s)
{
    size_t start = 0;
    while (start < s.size() && std::isspace((unsigned char)s[start])) start++;
    if (start >= s.size()) return {};

    size_t end = s.size();
    while (end > start && std::isspace((unsigned char)s[end - 1])) end--;

    return s.substr(start, end - start);
}

std::string cImGuiConsole::ToLower(const std::string& s)
{
    std::string o = s;
    std::transform(o.begin(), o.end(), o.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return o;
}

cImGuiConsole::tArgs cImGuiConsole::Tokenize(const std::string& s)
{
    tArgs out;
    std::string cur;

    for (char c : s)
    {
        if (std::isspace((unsigned char)c))
        {
            if (!cur.empty())
            {
                out.push_back(cur);
                cur.clear();
            }
        }
        else
        {
            cur.push_back(c);
        }
    }

    if (!cur.empty()) out.push_back(cur);
    return out;
}

// ----
// Public API
// ----
void cImGuiConsole::Init()
{
    mbVisible = false;
    mbLogExpanded = false;
    mbReclaimFocus = false;

    msInputBuf[0] = '\0';

    mvLog.clear();
    mvHistory.clear();
    mlHistoryPos = -1;

    mCommands.clear();
    RegisterBuiltinCommands();
}

void cImGuiConsole::Shutdown()
{
    mvLog.clear();
    mvHistory.clear();
    mCommands.clear();
    mCommandCallback = nullptr;
}

void cImGuiConsole::Toggle()
{
    mbVisible = !mbVisible;
    if (mbVisible)
        mbReclaimFocus = true;
}

void cImGuiConsole::SetVisible(bool abX)
{
    if (mbVisible == abX) return;
    Toggle();
}

void cImGuiConsole::ToggleLog()
{
    mbLogExpanded = !mbLogExpanded;
}

bool cImGuiConsole::IsVisible()
{
    return mbVisible;
}

bool cImGuiConsole::IsLogExpanded()
{
    return mbLogExpanded;
}

void cImGuiConsole::RegisterCompletion(const std::string& aName, tCompleteFn aFn)
{
    // Deliberately does NOT create the command. Attaching completion to a name
    // that was never registered would silently do nothing at Tab time, and the
    // log line says which name was wrong.
    auto it = mCommands.find(ToLower(aName));
    if (it == mCommands.end())
    {
        AddLog("RegisterCompletion: no command '%s'", aName.c_str());
        return;
    }
    it->second.mCompleteFn = std::move(aFn);
}

void cImGuiConsole::SetCommandCallback(std::function<void(const std::string&)> aCallback)
{
    mCommandCallback = std::move(aCallback);
}

void cImGuiConsole::RegisterCommand(const std::string& aName, const std::string& aHelp, tCommandFn aFn)
{
    const std::string key = ToLower(Trim(aName));
    if (key.empty()) return;

    sCommand cmd;
    cmd.msDisplayName = Trim(aName);
    cmd.msHelp = aHelp;
    cmd.mFn = std::move(aFn);
    mCommands[key] = std::move(cmd);
}

void cImGuiConsole::UnregisterCommand(const std::string& aName)
{
    const std::string key = ToLower(Trim(aName));
    if (key.empty()) return;
    mCommands.erase(key);
}

void cImGuiConsole::AddLog(const char* fmt, ...)
{
    if (!fmt) return;

    char buf[2048];
    va_list args;
    va_start(args, fmt);
#if defined(_MSC_VER)
    vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, args);
#else
    vsnprintf(buf, sizeof(buf), fmt, args);
#endif
    va_end(args);

    mvLog.emplace_back(buf);

    const size_t kMaxLines = 2000;
    if (mvLog.size() > kMaxLines)
        mvLog.erase(mvLog.begin(), mvLog.begin() + (mvLog.size() - kMaxLines));
}

void cImGuiConsole::ClearLog()
{
    mvLog.clear();
}

// ----
// Command execution
// ----
void cImGuiConsole::RegisterVariable(const std::string& aName, const std::string& aHelp,
                                    tGetValueFn aGetFn, tSetValueFn aSetFn)
{
    sVariable var;
    var.msDisplayName = Trim(aName);
    var.msHelp = aHelp;
    var.mGetFn = aGetFn;
    var.mSetFn = aSetFn;

    mVariables[ToLower(Trim(aName))] = var;
}

void cImGuiConsole::UnregisterVariable(const std::string& aName)
{
    mVariables.erase(ToLower(aName));
}

bool cImGuiConsole::GetVariableValue(const std::string& aName, std::string& asOut)
{
    auto it = mVariables.find(ToLower(aName));
    if (it == mVariables.end() || !it->second.mGetFn)
        return false;

    asOut = it->second.mGetFn();
    return true;
}

bool cImGuiConsole::IsVariable(const std::string& asName)
{
    return mVariables.find(ToLower(asName)) != mVariables.end();
}

bool cImGuiConsole::GhostIsFresh()
{
    if (msGhostText.empty()) return false;

    const std::string sLine(msInputBuf);

    //Still green only while the line is exactly "what was there" + "what Tab
    //added" and nothing else. Type, backspace or move on and it is just text.
    return (int)sLine.size() == mlGhostStart + (int)msGhostText.size() &&
           sLine.compare(mlGhostStart, msGhostText.size(), msGhostText) == 0;
}

void cImGuiConsole::ExecLine(const std::string& aLine)
{
    ///////////////////////////////////////////////////////////////////
    // A bare variable name reads it; a name followed by anything writes it.
    // Checked before commands so a variable cannot be shadowed by one.
    {
        const tArgs vParts = Tokenize(Trim(aLine));
        if (!vParts.empty())
        {
            auto it = mVariables.find(ToLower(vParts[0]));
            if (it != mVariables.end())
            {
                if (vParts.size() == 1)
                {
                    AddLog("%s = %s", vParts[0].c_str(),
                           it->second.mGetFn ? it->second.mGetFn().c_str() : "");
                    if (!it->second.msHelp.empty())
                        AddLog("  %s", it->second.msHelp.c_str());
                }
                else if (!it->second.mSetFn)
                {
                    AddLog("%s is read only", vParts[0].c_str());
                }
                else
                {
                    //Everything after the name, joined back up, so a value with
                    //spaces survives the round trip.
                    std::string sValue;
                    for (size_t i = 1; i < vParts.size(); ++i)
                    {
                        if (!sValue.empty()) sValue += " ";
                        sValue += vParts[i];
                    }

                    it->second.mSetFn(sValue);
                }

                return;
            }
        }
    }

    if (TryExecBuiltin(aLine))
        return;

    if (mCommandCallback)
        mCommandCallback(aLine);
    else
        AddLog("Unknown command. Type 'help' for list.");
}

bool cImGuiConsole::TryExecBuiltin(const std::string& aLine)
{
    const std::string line = Trim(aLine);
    if (line.empty())
        return true;

    tArgs parts = Tokenize(line);
    if (parts.empty())
        return true;

    const std::string cmdName = ToLower(parts[0]);
    auto it = mCommands.find(cmdName);
    if (it == mCommands.end())
        return false;

    tArgs cmdArgs;
    if (parts.size() > 1)
        cmdArgs.assign(parts.begin() + 1, parts.end());

    if (it->second.mFn)
        it->second.mFn(cmdArgs);

    return true;
}

void cImGuiConsole::RegisterBuiltinCommands()
{
    RegisterCommand("help", "help | Lists available console commands", [](const tArgs&)
    {
        AddLog("Commands:");
        for (const auto& kv : mCommands)
        {
            if (!kv.second.msHelp.empty())
                AddLog("  %s - %s", kv.first.c_str(), kv.second.msHelp.c_str());
            else
                AddLog("  %s", kv.first.c_str());
        }
    });

    RegisterCommand("clear", "clear | Clears the console output", [](const tArgs&)
    {
        ClearLog();
    });
}

// ----
// UI
// ----
void cImGuiConsole::Draw()
{
    if (!mbVisible)
        return;

    // --- DISABLE ImGui mouse cursor entirely ---
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    ImGui::SetMouseCursor(ImGuiMouseCursor_None);

    const float displayW = io.DisplaySize.x;
    const float displayH = io.DisplaySize.y;

    // Scaling based on 1440p reference.
    const float scale = displayH / 1440.0f;
    const float barHeight = 24.0f * scale;
    const float outlineThick = 2.0f * scale;
    const float margin = 12.0f * scale;

    // Log panel only when expanded via Shift+§
    const bool showLog = mbLogExpanded && !mvLog.empty();
    const float logHeight = showLog ? (220.0f * scale) : 0.0f;

    ///////////////////////////////////////////////////////////////////
    // Type-ahead drop-down, rebuilt from the line every frame.
    RefreshSuggestions();

    const float lineHeight = ImGui::GetTextLineHeight();
    const float suggestPad = 4.0f * scale;

    //Rows, plus one more for the "too many to show" line when there is one.
    int lSuggestRows = (int)mvSuggestions.size();
    const bool bTooMany = mlSuggestionTotal > kMaxSuggestionRows;
    if (bTooMany) lSuggestRows = 1;

    const bool showSuggest = lSuggestRows > 0;
    const float suggestHeight = showSuggest ? (lineHeight * lSuggestRows + suggestPad * 2.0f) : 0.0f;

    const float totalHeight = (barHeight + outlineThick * 2.0f) + suggestHeight + logHeight;

    // Colors -- the console's own palette, not repainted.
    const ImU32 colOutline = IM_COL32(0x20, 0x20, 0x1A, 0xFF);
    const ImU32 colBarBg   = IM_COL32(0x40, 0x40, 0x33, 0xFF);
    const ImU32 colLogBg   = IM_COL32(0x28, 0x28, 0x21, 0xF0);
    const ImU32 colSuggestBg = IM_COL32(0x33, 0x33, 0x2A, 0xF6);

    //Commands read as actions and variables as values, so they are told apart
    //by colour the way the rest of the bar already tells its parts apart.
    const ImU32 colCommand = IM_COL32(0x7A, 0xB8, 0xFF, 0xFF);
    const ImU32 colVarName = IM_COL32(0xE6, 0xE6, 0xD9, 0xFF);
    const ImU32 colVarValue= IM_COL32(0xBF, 0xB8, 0x73, 0xFF);
    const ImU32 colTooMany = IM_COL32(0xA8, 0xA2, 0x80, 0xFF);
    const ImU32 colGhost   = IM_COL32(0x59, 0xF2, 0x59, 0xFF);

    // Window
    ImGui::SetNextWindowPos(ImVec2(margin, margin));
    ImGui::SetNextWindowSize(ImVec2(displayW - margin * 2.0f, totalHeight));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                    ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (!ImGui::Begin("##HPL2Console", nullptr, flags))
    {
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Outer outline
    drawList->AddRectFilled(
        ImVec2(margin, margin),
        ImVec2(displayW - margin, margin + totalHeight),
        colOutline);

    // Bar background
    drawList->AddRectFilled(
        ImVec2(margin + outlineThick, margin + outlineThick),
        ImVec2(displayW - margin - outlineThick, margin + outlineThick + barHeight),
        colBarBg);

    // Suggestion drop-down background
    const float suggestTop = margin + outlineThick + barHeight;
    if (showSuggest)
    {
        drawList->AddRectFilled(
            ImVec2(margin + outlineThick, suggestTop),
            ImVec2(displayW - margin - outlineThick, suggestTop + suggestHeight),
            colSuggestBg);
    }

    // Log background (only when expanded)
    if (showLog)
    {
        const float logTop = margin + outlineThick + barHeight + suggestHeight;
        drawList->AddRectFilled(
            ImVec2(margin + outlineThick, logTop),
            ImVec2(displayW - margin - outlineThick, logTop + logHeight - outlineThick),
            colLogBg);
    }

    // Prefix: "HPL2  x64 >"
    const float textY = margin + outlineThick + (barHeight - ImGui::GetFontSize()) * 0.5f;
    float cursorX = margin + outlineThick + 6.0f * scale;

    const ImVec4 colHPL2 = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
    const ImVec4 colX64  = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
    const ImVec4 colGt   = ImVec4(1.0f, 1.0f, 0.502f, 1.0f);
    const ImVec4 colText = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    const char* partHPL2 = "HPL2  ";
    const char* partX64  = "x64 ";
    const char* partGt   = ">";

    drawList->AddText(ImVec2(cursorX, textY), ImGui::ColorConvertFloat4ToU32(colHPL2), partHPL2);
    cursorX += ImGui::CalcTextSize(partHPL2).x;

    drawList->AddText(ImVec2(cursorX, textY), ImGui::ColorConvertFloat4ToU32(colX64), partX64);
    cursorX += ImGui::CalcTextSize(partX64).x;

    drawList->AddText(ImVec2(cursorX, textY), ImGui::ColorConvertFloat4ToU32(colGt), partGt);
    cursorX += ImGui::CalcTextSize(partGt).x;

    cursorX += ImGui::CalcTextSize("   ").x;

    // InputText
    const float inputWidth = displayW - cursorX - outlineThick - 4.0f * scale;

    ImGui::SetCursorPos(ImVec2(cursorX - margin, outlineThick + (barHeight - ImGui::GetFontSize()) * 0.5f));

    ImGui::PushItemWidth(inputWidth);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, colText);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.3f, 0.3f, 0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

    const ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue |
                    ImGuiInputTextFlags_CallbackHistory |
                    ImGuiInputTextFlags_CallbackCompletion;

    if (mbReclaimFocus)
    {
        ImGui::SetKeyboardFocusHere(0);
        mbReclaimFocus = false;
    }
    const bool entered = ImGui::InputText("##consoleinput", msInputBuf, sizeof(msInputBuf), inputFlags, TextEditCallback);

    //Where the typed text ACTUALLY starts, straight from the widget that just
    //drew it, rather than re-deriving it from the prompt width and hoping the
    //two agree. Frame padding is zeroed above, so the item's left edge is the
    //text's left edge. Everything that has to line up under the input -- the
    //green tail, the suggestion rows -- hangs off this.
    const float inputTextX = ImGui::GetItemRectMin().x;
    const float inputTextY = ImGui::GetItemRectMin().y;

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);
    ImGui::PopItemWidth();

    ///////////////////////////////////////////////////////////////////
    // Recolour the part Tab wrote. The text is already in the buffer and
    // already painted white by InputText, so this draws the same glyphs at the
    // same place in green over the top -- opaque glyphs, so they simply cover.
    // Nothing to undo when it stops being fresh: the colour goes, the text
    // stays, and typing carries straight on from it.
    if (GhostIsFresh())
    {
        const std::string sPrefix(msInputBuf, msInputBuf + mlGhostStart);

        drawList->AddText(
            ImVec2(inputTextX + ImGui::CalcTextSize(sPrefix.c_str()).x, inputTextY),
            colGhost, msGhostText.c_str());
    }

    if (entered)
    {
        std::string cmd(msInputBuf);
        cmd = Trim(cmd);

        msGhostText.clear();
        msGhostStem.clear();

        if (!cmd.empty())
        {
            mvHistory.push_back(cmd);
            mlHistoryPos = -1;

            AddLog("HPL2 x64 >%s", cmd.c_str());
            ExecLine(cmd);
        }

        msInputBuf[0] = '\0';
        mbReclaimFocus = true;
    }

    ///////////////////////////////////////////////////////////////////
    // Suggestion rows. A variable shows what it currently IS on the right --
    // the point of offering it is usually to find that out.
    if (showSuggest)
    {
        //Under the text being typed, not against the window edge -- the list is
        //about that word, so it reads as belonging to it.
        const float rowX = inputTextX;

        //Values start in one column, LEFT aligned, sized to clear the longest
        //name on show. Right-aligning them made every value end in the same
        //place and start somewhere different, which is the ragged edge you
        //actually read down.
        float nameColW = 0.0f;
        for (size_t i = 0; i < mvSuggestions.size(); ++i)
        {
            const float w = ImGui::CalcTextSize(mvSuggestions[i].c_str()).x;
            if (w > nameColW) nameColW = w;
        }

        const float valueX = rowX + nameColW + 40.0f * scale;
        float rowY = suggestTop + suggestPad;

        if (bTooMany)
        {
            char sMsg[192];
            snprintf(sMsg, sizeof(sMsg),
                "%d matches (too many to show here, press shift+tilde to open full console)",
                mlSuggestionTotal);

            drawList->AddText(ImVec2(rowX, rowY), colTooMany, sMsg);
        }
        else
        {
            for (size_t i = 0; i < mvSuggestions.size(); ++i)
            {
                const std::string& sName = mvSuggestions[i];
                const bool bIsVar = IsVariable(sName);

                drawList->AddText(ImVec2(rowX, rowY),
                                  bIsVar ? colVarName : colCommand, sName.c_str());

                if (bIsVar)
                {
                    std::string sValue;
                    if (GetVariableValue(sName, sValue) && !sValue.empty())
                        drawList->AddText(ImVec2(valueX, rowY), colVarValue, sValue.c_str());
                }

                rowY += lineHeight;
            }
        }
    }

    // Log lines — only when expanded
    if (showLog)
    {
        const float logTop = outlineThick + barHeight + suggestHeight + 6.0f * scale;
        ImGui::SetCursorPos(ImVec2(outlineThick + 8.0f * scale, logTop));

        const int maxLines = 12;
        const int start = (int)mvLog.size() > maxLines ? (int)mvLog.size() - maxLines : 0;
        for (int i = start; i < (int)mvLog.size(); ++i)
            ImGui::TextUnformatted(mvLog[i].c_str());
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void cImGuiConsole::CollectMatches(const std::string& asWord, std::vector<std::string>& avOut)
{
    avOut.clear();

    const std::string sWordLower = ToLower(asWord);

    //Matched on the lowercase key, but what comes back is the name as it was
    //REGISTERED -- that is what the drop-down shows and what Tab fills in.
    for (auto it = mVariables.begin(); it != mVariables.end(); ++it)
    {
        if (it->first.compare(0, sWordLower.size(), sWordLower) == 0)
            avOut.push_back(it->second.msDisplayName.empty() ? it->first : it->second.msDisplayName);
    }

    for (auto it = mCommands.begin(); it != mCommands.end(); ++it)
    {
        if (it->first.compare(0, sWordLower.size(), sWordLower) == 0)
        {
            //A name registered as both is offered once, as the variable: that is
            //the entry that can show a value, and ExecLine resolves it that way
            //too, so the list must not claim otherwise.
            if (mVariables.find(it->first) != mVariables.end()) continue;

            avOut.push_back(it->second.msDisplayName.empty() ? it->first : it->second.msDisplayName);
        }
    }

    //Case-insensitive, or every capital letter would sort the name away from
    //its lowercase neighbours and the list would look shuffled.
    std::sort(avOut.begin(), avOut.end(),
        [](const std::string& a, const std::string& b) { return ToLower(a) < ToLower(b); });
}

void cImGuiConsole::RefreshSuggestions()
{
    mvSuggestions.clear();
    mlSuggestionTotal = 0;

    const std::string sLine(msInputBuf);

    //Only the first word gets a drop-down. Past that the line is arguments, and
    //what those can be is the command's business, not a global name list.
    if (sLine.find_first_of(" \t") != std::string::npos)
        return;

    //An empty line would match everything, which is a list nobody asked for.
    if (sLine.empty())
        return;

    std::vector<std::string> vMatches;
    CollectMatches(sLine, vMatches);

    mlSuggestionTotal = (int)vMatches.size();

    //An exact and only match is not a suggestion, it is what you already typed.
    if (mlSuggestionTotal == 1 && ToLower(sLine) == vMatches[0])
    {
        mlSuggestionTotal = 0;
        return;
    }

    for (int i = 0; i < mlSuggestionTotal && i < kMaxSuggestionRows; ++i)
        mvSuggestions.push_back(vMatches[i]);
}

void cImGuiConsole::HandleCompletion(ImGuiInputTextCallbackData* data)
{
    // Only ever complete the word the caret is sitting in, so Tab in the middle
    // of an already-typed line does not rewrite the tail.
    const std::string sLine(data->Buf, data->Buf + data->CursorPos);

    size_t lWordStart = sLine.find_last_of(" \t");
    lWordStart = (lWordStart == std::string::npos) ? 0 : lWordStart + 1;

    //Whether this Tab is a repeat of the last one has to be decided BEFORE the
    //buffer is touched, and cycling completes from the original stem rather
    //than from what the previous Tab wrote.
    const bool bWasFresh = GhostIsFresh();

    const std::string sWord = bWasFresh ? msGhostStem : sLine.substr(lWordStart);

    std::vector<std::string> vCandidates;

    // Nothing but the first word typed so far: complete the NAME itself.
    const bool bFirstWord = (Trim(sLine.substr(0, lWordStart)).empty());

    if (bFirstWord)
    {
        CollectMatches(sWord, vCandidates);
    }
    else
    {
        // Otherwise ask the command what its arguments can be.
        const tArgs vTokens = Tokenize(sLine);
        if (vTokens.empty()) return;

        auto it = mCommands.find(ToLower(vTokens[0]));
        if (it == mCommands.end() || !it->second.mCompleteFn) return;

        vCandidates = it->second.mCompleteFn(sWord);
        std::sort(vCandidates.begin(), vCandidates.end());
    }

    if (vCandidates.empty())
    {
        msGhostText.clear();
        return;
    }

    ///////////////////////////////////////////////////////////////////
    // Tab writes the completion into the line for real -- it is text you can
    // carry on typing onto, not a suggestion that evaporates. It is only drawn
    // green, and only for as long as the line still ends in exactly it.
    //
    // Tab again walks to the NEXT match. That means completing from what was
    // typed before the first Tab, not from Tab's own output, which is what
    // msGhostStem remembers.
    //Tab replaces the word in place, so the word start does not move between
    //repeats -- freshness alone says whether this is a repeat.
    if (bWasFresh)
    {
        mlGhostMatch = (mlGhostMatch + 1) % (int)vCandidates.size();
    }
    else
    {
        mlGhostMatch = 0;
        msGhostStem = sWord;
    }

    const std::string& sPick = vCandidates[mlGhostMatch];

    //Replace the whole word with the match.
    data->DeleteChars((int)lWordStart, (int)(data->CursorPos - lWordStart));
    data->InsertChars((int)lWordStart, sPick.c_str());

    mlGhostStart = (int)(lWordStart + msGhostStem.size());

    //A candidate shorter than the stem (an argument provider is free to return
    //anything) has no tail to colour.
    if (sPick.size() >= msGhostStem.size())
        msGhostText = sPick.substr(msGhostStem.size());
    else
        msGhostText.clear();

    //The full list still goes to the log, so shift+tilde shows what the
    //drop-down had to leave out.
    if (vCandidates.size() > (size_t)kMaxSuggestionRows)
    {
        std::string sList;
        for (size_t i = 0; i < vCandidates.size(); ++i)
        {
            if (!sList.empty()) sList += "  ";
            sList += vCandidates[i];
        }

        AddLog("%d matches: %s", (int)vCandidates.size(), sList.c_str());
    }
}

int cImGuiConsole::TextEditCallback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
    {
        HandleCompletion(data);
        return 0;
    }

    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
    {
        if (mvHistory.empty())
            return 0;

        const int prevPos = mlHistoryPos;

        if (data->EventKey == ImGuiKey_UpArrow)
        {
            if (mlHistoryPos == -1)
                mlHistoryPos = (int)mvHistory.size() - 1;
            else if (mlHistoryPos > 0)
                mlHistoryPos--;
        }
        else if (data->EventKey == ImGuiKey_DownArrow)
        {
            if (mlHistoryPos != -1)
            {
                mlHistoryPos++;
                if (mlHistoryPos >= (int)mvHistory.size())
                    mlHistoryPos = -1;
            }
        }

        if (prevPos != mlHistoryPos)
        {
            const char* historyStr = (mlHistoryPos >= 0) ? mvHistory[mlHistoryPos].c_str() : "";
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, historyStr);
        }
    }

    return 0;
}