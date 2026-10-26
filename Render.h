#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <string>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_addons.h"
#include "ImGui/colors.h"
#include "../Core/Vars/Vars.h"
#include "../Core/Config/Config.h"
#include <vector>
#include <mutex>
#include <chrono>
#include "../Core/Globals/Globals.h"
#include <algorithm>
#include "../Core/Cache/Cache.h"
#include "../Core/Features/Music/Music.h"
#include "../Core/Features/Music/MusicPlayer.h" 
#include "../Core/Features/Servers/Servers.h"
#include "../Core/Features/Explorer/Explorer.h"
#include "../Core/Features/Executor/MonacoEditor.h"
#include <cctype>
#include <cmath>

#include <filesystem>
#include <fstream>
#include <functional>
#include <array>
#include <ctime>
#include <sstream>
#include <cstring>
#include <cstdio>

#include "../Core/Features/RemoteSpy/RemoteSpy.h"

#include "../Core/Features/Visuals/Visuals.h"
#include <winhttp.h>
#include <wincodec.h>

#include <thread>
#include <urlmon.h>

#pragma comment(lib, "urlmon.lib")

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "windowscodecs.lib")

// ============================================================
// Shrimplock Executor UI v2
//
// DROP-IN PARTS:
// 1) Add the includes below near your existing includes.
// 2) Paste namespace ExecutorUI BEFORE: class OverlayWindow
// 3) Replace ONLY your existing OverlayWindow::RenderLuaEditor()
//    with the RenderLuaEditor() function at the bottom.
//
// This is the UI/editor shell. RunHandler intentionally stays as
// a callback so you can connect it to a Lua/Luau runtime you own.
// ============================================================


// ============================================================
// EXTRA INCLUDES
// ============================================================

#include <filesystem>
#include <fstream>
#include <functional>
#include <array>
#include <ctime>
#include <sstream>
#include <cstring>
#include <cstdio>
#include <system_error>
#include <shellapi.h>

#pragma comment(lib, "Shell32.lib")


// ============================================================
// EXECUTOR STATE / FILES / LOGS / COMPLETION
// Paste this BEFORE: class OverlayWindow
// ============================================================

// ============================================================
// SHRIMPLOCK EXECUTOR - MATCHA-STYLE LAYOUT
// Original branding/content, but the layout intentionally follows
// the compact IDE structure from the reference screenshot.
//
// REPLACE:
//   1) your current namespace ExecutorUI { ... }
//   2) your current OverlayWindow::RenderLuaEditor()
//
// KEEP:
//   - your existing <> utility-bar button
//   - the rest of OverlayWindow
//
// REQUIRED INCLUDES (add once near the top of your file):
// #include <filesystem>
// #include <fstream>
// #include <functional>
// #include <array>
// #include <ctime>
// #include <sstream>
// #include <cstring>
// #include <cstdio>
// #include <system_error>
// #include <shellapi.h>
// #pragma comment(lib, "Shell32.lib")
// ============================================================


namespace ExecutorUI
{
    enum class LogType
    {
        Info,
        Print,
        Success,
        Warning,
        Error
    };

    struct LogEntry
    {
        LogType type = LogType::Info;
        std::string script;
        std::string text;
        std::chrono::system_clock::time_point timestamp;
    };

    struct ScriptTab
    {
        std::string name = "untitled.lua";
        std::filesystem::path path;
        std::array<char, 65536> buffer{};
        bool dirty = false;

        ScriptTab()
        {
            buffer[0] = '\0';
        }
    };

    using RunHandlerType =
        std::function<void(
            const std::string& scriptName,
            const std::string& code
            )>;

    inline RunHandlerType RunHandler{};

    inline std::vector<ScriptTab> Tabs{};
    inline int CurrentTab = 0;

    inline std::vector<LogEntry> Logs{};
    inline std::mutex LogMutex{};

    inline bool ShowScripts = true;
    inline bool ShowTerminal = true;
    inline bool AutoScroll = true;
    inline bool ScrollToBottom = false;

    inline bool SaveAsPopup = false;
    inline char SaveAsName[128] = "untitled.lua";

    inline int CursorLine = 1;
    inline int CursorColumn = 1;

    // ------------------------------------------------------------
    // THEME
    // ------------------------------------------------------------

    inline ImVec4 Accent(float alpha = 1.0f)
    {
        return ImVec4(
            Vars::Settings::themeColor[0],
            Vars::Settings::themeColor[1],
            Vars::Settings::themeColor[2],
            alpha
        );
    }

    inline ImVec4 Mix(
        const ImVec4& a,
        const ImVec4& b,
        float t
    )
    {
        t = std::clamp(t, 0.0f, 1.0f);

        return ImVec4(
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
            a.w + (b.w - a.w) * t
        );
    }

    inline ImVec4 WindowBg()
    {
        return ImVec4(0.025f, 0.026f, 0.030f, 1.0f);
    }

    inline ImVec4 PanelBg()
    {
        return ImVec4(0.033f, 0.034f, 0.039f, 1.0f);
    }

    inline ImVec4 EditorBg()
    {
        return ImVec4(0.020f, 0.021f, 0.024f, 1.0f);
    }

    inline ImVec4 TopBarBg()
    {
        return ImVec4(0.043f, 0.044f, 0.050f, 1.0f);
    }

    inline ImVec4 Border()
    {
        return ImVec4(0.110f, 0.115f, 0.130f, 1.0f);
    }

    inline ImVec4 AccentSoft(float strength = 0.22f)
    {
        return Mix(
            ImVec4(0.055f, 0.057f, 0.065f, 1.0f),
            Accent(),
            strength
        );
    }

    // ------------------------------------------------------------
    // FILES
    // ------------------------------------------------------------

    inline std::filesystem::path ScriptsFolder()
    {
        wchar_t exePath[MAX_PATH]{};

        GetModuleFileNameW(
            nullptr,
            exePath,
            MAX_PATH
        );

        auto folder =
            std::filesystem::path(exePath)
            .parent_path()
            / L"scripts";

        std::error_code ec;

        std::filesystem::create_directories(
            folder,
            ec
        );

        return folder;
    }

    inline void OpenScriptsFolder()
    {
        auto folder = ScriptsFolder();

        ShellExecuteW(
            nullptr,
            L"open",
            folder.c_str(),
            nullptr,
            nullptr,
            SW_SHOWNORMAL
        );
    }

    inline void EnsureTab()
    {
        if (Tabs.empty())
        {
            Tabs.emplace_back();
            CurrentTab = 0;
        }

        if (
            CurrentTab < 0 ||
            CurrentTab >= static_cast<int>(Tabs.size())
            )
        {
            CurrentTab = 0;
        }
    }

    inline ScriptTab& Current()
    {
        EnsureTab();
        return Tabs[CurrentTab];
    }

    inline std::string Text(
        const ScriptTab& tab
    )
    {
        return std::string(
            tab.buffer.data()
        );
    }

    inline void NewScript()
    {
        Tabs.emplace_back();

        CurrentTab =
            static_cast<int>(Tabs.size()) - 1;

        std::lock_guard<std::mutex> lock(
            LogMutex
        );

        Logs.push_back(
            {
                LogType::Info,
                Tabs[CurrentTab].name,
                "Created a new script.",
                std::chrono::system_clock::now()
            }
        );

        ScrollToBottom = true;
    }

    // ------------------------------------------------------------
    // LOGS
    // ------------------------------------------------------------

    inline const char* LogLabel(
        LogType type
    )
    {
        switch (type)
        {
        case LogType::Info:
            return "INFO";

        case LogType::Print:
            return "PRINT";

        case LogType::Success:
            return "OK";

        case LogType::Warning:
            return "WARN";

        case LogType::Error:
            return "ERROR";
        }

        return "LOG";
    }

    inline ImVec4 LogColor(
        LogType type
    )
    {
        switch (type)
        {
        case LogType::Info:
            return Mix(
                ImVec4(0.42f, 0.54f, 0.78f, 1.0f),
                Accent(),
                0.30f
            );

        case LogType::Print:
            return ImVec4(
                0.80f,
                0.81f,
                0.84f,
                1.0f
            );

        case LogType::Success:
            return ImVec4(
                0.39f,
                0.80f,
                0.52f,
                1.0f
            );

        case LogType::Warning:
            return ImVec4(
                0.94f,
                0.69f,
                0.27f,
                1.0f
            );

        case LogType::Error:
            return ImVec4(
                0.94f,
                0.36f,
                0.39f,
                1.0f
            );
        }

        return ImVec4(1, 1, 1, 1);
    }

    inline std::string TimeString(
        const std::chrono::system_clock::time_point& tp
    )
    {
        std::time_t raw =
            std::chrono::system_clock::to_time_t(tp);

        std::tm local{};

        localtime_s(
            &local,
            &raw
        );

        char buffer[16]{};

        std::strftime(
            buffer,
            sizeof(buffer),
            "%H:%M:%S",
            &local
        );

        return buffer;
    }

    inline void PushLog(
        LogType type,
        const std::string& text,
        const std::string& script = {}
    )
    {
        EnsureTab();

        std::lock_guard<std::mutex> lock(
            LogMutex
        );

        Logs.push_back(
            {
                type,
                script.empty()
                    ? Current().name
                    : script,
                text,
                std::chrono::system_clock::now()
            }
        );

        if (Logs.size() > 800)
        {
            Logs.erase(
                Logs.begin(),
                Logs.begin() + 100
            );
        }

        ScrollToBottom = true;
    }

    inline std::vector<LogEntry>
        CurrentLogs()
    {
        EnsureTab();

        const std::string script =
            Current().name;

        std::vector<LogEntry> result;

        std::lock_guard<std::mutex> lock(
            LogMutex
        );

        for (const auto& log : Logs)
        {
            if (log.script == script)
                result.push_back(log);
        }

        return result;
    }

    inline void ClearCurrentLogs()
    {
        EnsureTab();

        const std::string script =
            Current().name;

        std::lock_guard<std::mutex> lock(
            LogMutex
        );

        Logs.erase(
            std::remove_if(
                Logs.begin(),
                Logs.end(),
                [&](const LogEntry& log)
                {
                    return log.script == script;
                }
            ),
            Logs.end()
        );
    }

    // ------------------------------------------------------------
    // SAVE / LOAD
    // ------------------------------------------------------------

    inline bool SaveTo(
        ScriptTab& tab,
        std::filesystem::path requestedPath
    )
    {
        if (
            requestedPath.extension() !=
            ".lua"
            )
        {
            requestedPath += ".lua";
        }

        auto finalPath =
            ScriptsFolder()
            / requestedPath.filename();

        std::ofstream out(
            finalPath,
            std::ios::binary |
            std::ios::trunc
        );

        if (!out.is_open())
        {
            PushLog(
                LogType::Error,
                "Could not save script.",
                tab.name
            );

            return false;
        }

        out.write(
            tab.buffer.data(),
            static_cast<std::streamsize>(
                std::strlen(
                    tab.buffer.data()
                )
                )
        );

        out.close();

        tab.path = finalPath;
        tab.name =
            finalPath.filename().string();
        tab.dirty = false;

        PushLog(
            LogType::Success,
            "Saved " + tab.name,
            tab.name
        );

        return true;
    }

    inline bool SaveCurrent()
    {
        auto& tab = Current();

        if (tab.path.empty())
            return false;

        return SaveTo(
            tab,
            tab.path
        );
    }

    inline bool LoadFile(
        const std::filesystem::path& path
    )
    {
        for (
            int i = 0;
            i < static_cast<int>(Tabs.size());
            ++i
            )
        {
            if (
                !Tabs[i].path.empty() &&
                Tabs[i].path == path
                )
            {
                CurrentTab = i;
                return true;
            }
        }

        std::ifstream in(
            path,
            std::ios::binary
        );

        if (!in.is_open())
            return false;

        std::ostringstream stream;
        stream << in.rdbuf();

        std::string text =
            stream.str();

        if (text.size() >= 65535)
            text.resize(65534);

        ScriptTab tab;

        tab.buffer.fill('\0');

        std::copy(
            text.begin(),
            text.end(),
            tab.buffer.begin()
        );

        tab.buffer[text.size()] =
            '\0';

        tab.path = path;
        tab.name =
            path.filename().string();

        tab.dirty = false;

        Tabs.push_back(
            std::move(tab)
        );

        CurrentTab =
            static_cast<int>(Tabs.size()) - 1;

        PushLog(
            LogType::Info,
            "Opened " + Current().name,
            Current().name
        );

        return true;
    }

    inline std::vector<
        std::filesystem::path
    > ScriptFiles()
    {
        std::vector<
            std::filesystem::path
        > result;

        std::error_code ec;

        auto folder =
            ScriptsFolder();

        for (
            const auto& entry :
            std::filesystem::directory_iterator(
                folder,
                ec
            )
            )
        {
            if (ec)
                break;

            if (
                entry.is_regular_file() &&
                entry.path().extension() ==
                ".lua"
                )
            {
                result.push_back(
                    entry.path()
                );
            }
        }

        std::sort(
            result.begin(),
            result.end(),
            [](const auto& a, const auto& b)
            {
                return
                    a.filename().string() <
                    b.filename().string();
            }
        );

        return result;
    }

    // ------------------------------------------------------------
    // COMPLETION / CURSOR
    // ------------------------------------------------------------

    inline const std::vector<std::string>&
        CompletionWords()
    {
        static const std::vector<std::string>
            words =
        {
            "and", "break", "do", "else", "elseif",
            "end", "false", "for", "function", "if",
            "in", "local", "nil", "not", "or",
            "repeat", "return", "then", "true",
            "until", "while",

            "assert", "error", "ipairs", "next",
            "pairs", "pcall", "print", "select",
            "tonumber", "tostring", "type", "typeof",
            "warn", "xpcall",

            "coroutine", "math", "string", "table",
            "utf8", "Vector2", "Vector3", "CFrame",
            "Color3", "Instance", "game", "workspace",
            "script", "require"
        };

        return words;
    }

    inline void UpdateCursorInfo(
        ImGuiInputTextCallbackData* data
    )
    {
        CursorLine = 1;
        CursorColumn = 1;

        for (
            int i = 0;
            i < data->CursorPos;
            ++i
            )
        {
            if (data->Buf[i] == '\n')
            {
                ++CursorLine;
                CursorColumn = 1;
            }
            else
            {
                ++CursorColumn;
            }
        }
    }

    inline int EditorCallback(
        ImGuiInputTextCallbackData* data
    )
    {
        if (
            data->EventFlag ==
            ImGuiInputTextFlags_CallbackAlways
            )
        {
            UpdateCursorInfo(data);
            return 0;
        }

        if (
            data->EventFlag !=
            ImGuiInputTextFlags_CallbackCompletion
            )
        {
            return 0;
        }

        UpdateCursorInfo(data);

        std::string whole(
            data->Buf,
            data->BufTextLen
        );

        const int cursor =
            data->CursorPos;

        int start =
            cursor;

        while (start > 0)
        {
            const char c =
                whole[
                    static_cast<size_t>(
                        start - 1
                        )
                ];

            if (
                !std::isalnum(
                    static_cast<unsigned char>(c)
                ) &&
                c != '_'
                )
            {
                break;
            }

            --start;
        }

        const std::string word =
            whole.substr(
                static_cast<size_t>(start),
                static_cast<size_t>(
                    cursor - start
                    )
            );

        for (
            const auto& candidate :
            CompletionWords()
            )
        {
            if (
                !word.empty() &&
                candidate.size() > word.size() &&
                candidate.compare(
                    0,
                    word.size(),
                    word
                ) == 0
                )
            {
                data->DeleteChars(
                    start,
                    cursor - start
                );

                data->InsertChars(
                    start,
                    candidate.c_str()
                );

                return 0;
            }
        }

        data->InsertChars(
            cursor,
            "    "
        );

        return 0;
    }

    // ------------------------------------------------------------
    // RUN
    // ------------------------------------------------------------

    inline void RunCurrent()
    {
        auto& tab =
            Current();

        const std::string code =
            Text(tab);

        if (code.empty())
        {
            PushLog(
                LogType::Warning,
                "Nothing to run.",
                tab.name
            );

            return;
        }

        PushLog(
            LogType::Info,
            "Run requested.",
            tab.name
        );

        if (!RunHandler)
        {
            PushLog(
                LogType::Warning,
                "No local script runtime is connected.",
                tab.name
            );

            return;
        }

        RunHandler(
            tab.name,
            code
        );
    }
}

// ============================================================

struct FlingState
{
    bool active = false;
    std::string targetName;
    RBX::Vec3 oldPosition{};
    std::chrono::steady_clock::time_point startTime;
};

inline FlingState g_flingState;

struct Notification {
    std::string text;
    std::chrono::time_point<std::chrono::steady_clock> startTime;
    float duration;
};

class NotificationService
{
private:
    static inline std::vector<Notification> notifications;
    static inline std::mutex notifMutex;

public:
    static void Push(
        const std::string& text,
        float duration = 3.0f
    )
    {
        std::lock_guard<std::mutex> lock(notifMutex);

        notifications.push_back(
            {
                text,
                std::chrono::steady_clock::now(),
                duration
            }
        );
    }

    static void Render(ImDrawList* drawList)
    {
        std::lock_guard<std::mutex> lock(notifMutex);

        if (notifications.empty())
            return;

        auto now = std::chrono::steady_clock::now();

        ImVec2 displaySize = ImGui::GetIO().DisplaySize;

        // ============================================================
        // NOTIFICATION SETTINGS
        // ============================================================

        const float screenPadding = 22.0f;
        const float spacing = 12.0f;

        // Bigger notification
        const float minWidth = 330.0f;
        const float notificationHeight = 58.0f;

        const float horizontalPadding = 24.0f;

        const float fadeInTime = 0.30f;
        const float fadeOutTime = 0.35f;

        // Start from bottom-right
        float currentBottomY =
            displaySize.y - screenPadding;

        // ============================================================
        // MENU THEME COLOR
        // ============================================================

        ImVec4 themeColor(
            Vars::Settings::themeColor[0],
            Vars::Settings::themeColor[1],
            Vars::Settings::themeColor[2],
            1.0f
        );

        // ============================================================
        // REMOVE EXPIRED NOTIFICATIONS
        // ============================================================

        for (auto it = notifications.begin();
            it != notifications.end();)
        {
            float elapsed =
                std::chrono::duration<float>(
                    now - it->startTime
                ).count();

            if (elapsed >= it->duration)
            {
                it = notifications.erase(it);
                continue;
            }

            ++it;
        }

        if (notifications.empty())
            return;

        // ============================================================
        // RENDER FROM NEWEST -> OLDEST
        // ============================================================

        for (auto it = notifications.rbegin();
            it != notifications.rend();
            ++it)
        {
            float elapsed =
                std::chrono::duration<float>(
                    now - it->startTime
                ).count();

            float remaining =
                it->duration - elapsed;

            // ========================================================
            // FADE IN / FADE OUT
            // ========================================================

            float alpha = 1.0f;

            if (elapsed < fadeInTime)
            {
                alpha =
                    elapsed / fadeInTime;
            }
            else if (remaining < fadeOutTime)
            {
                alpha =
                    remaining / fadeOutTime;
            }

            alpha = std::clamp(
                alpha,
                0.0f,
                1.0f
            );

            // Smooth the fade
            alpha =
                alpha * alpha *
                (3.0f - 2.0f * alpha);

            // ========================================================
            // SIZE
            // ========================================================

            ImVec2 textSize =
                ImGui::CalcTextSize(
                    it->text.c_str()
                );

            float width =
                (std::max)(
                    minWidth,
                    textSize.x + horizontalPadding * 2.0f
                    );

            float height =
                notificationHeight;

            // ========================================================
            // POSITION - BOTTOM RIGHT
            // ========================================================

            float x =
                displaySize.x -
                width -
                screenPadding;

            float y =
                currentBottomY -
                height;

            ImVec2 pos(
                x,
                y
            );

            ImVec2 end(
                x + width,
                y + height
            );

            // ========================================================
            // COLORS
            // ========================================================

            int bgAlpha =
                static_cast<int>(
                    235.0f * alpha
                    );

            int borderAlpha =
                static_cast<int>(
                    180.0f * alpha
                    );

            int themeAlpha =
                static_cast<int>(
                    255.0f * alpha
                    );

            int textAlpha =
                static_cast<int>(
                    255.0f * alpha
                    );

            ImU32 accentColor =
                ImGui::ColorConvertFloat4ToU32(
                    ImVec4(
                        themeColor.x,
                        themeColor.y,
                        themeColor.z,
                        alpha
                    )
                );

            // ========================================================
            // SHADOW
            // ========================================================

            drawList->AddRectFilled(
                pos + ImVec2(3.0f, 4.0f),
                end + ImVec2(3.0f, 4.0f),
                IM_COL32(
                    0,
                    0,
                    0,
                    static_cast<int>(
                        90.0f * alpha
                        )
                ),
                8.0f
            );

            // ========================================================
            // BACKGROUND
            // ========================================================

            drawList->AddRectFilled(
                pos,
                end,
                IM_COL32(
                    18,
                    18,
                    20,
                    bgAlpha
                ),
                8.0f
            );

            // ========================================================
            // THEME COLORED BORDER
            // ========================================================

            drawList->AddRect(
                pos,
                end,
                accentColor,
                8.0f,
                0,
                1.5f
            );

            // ========================================================
            // THEME ACCENT BAR
            // ========================================================

            drawList->AddRectFilled(
                ImVec2(
                    pos.x,
                    pos.y + 7.0f
                ),
                ImVec2(
                    pos.x + 5.0f,
                    end.y - 7.0f
                ),
                accentColor,
                3.0f
            );

            // ========================================================
            // TEXT
            // ========================================================

            ImVec2 textPos(
                pos.x + 20.0f,
                pos.y +
                (height - textSize.y) * 0.5f
            );

            drawList->AddText(
                textPos,
                IM_COL32(
                    235,
                    235,
                    238,
                    textAlpha
                ),
                it->text.c_str()
            );

            // Next notification goes ABOVE this one
            currentBottomY -=
                height + spacing;
        }
    }
};

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK OverlayWndProc(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
)
{
    // Don't steal Roblox focus while the main menu is closed.
    if (msg == WM_MOUSEACTIVATE)
    {
        if (!Vars::menuOpen)
            return MA_NOACTIVATE;

        return MA_ACTIVATE;
    }

    if (ImGui_ImplWin32_WndProcHandler(
        hWnd,
        msg,
        wParam,
        lParam))
    {
        return true;
    }

    if (msg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(
        hWnd,
        msg,
        wParam,
        lParam
    );
}

class OverlayWindow {
private:
    HWND windowHandle;
    WNDCLASSEXW windowClass;

    ID3D11Device* d3dDevice;
    ID3D11DeviceContext* d3dContext;
    IDXGISwapChain* swapChain;
    ID3D11RenderTargetView* renderTarget;

    ID3D11ShaderResourceView* loadingLogoTexture = nullptr;

    bool loadingAnimationStarted = false;

    std::chrono::steady_clock::time_point loadingAnimationStart;

    bool LoadLoadingLogo()
    {
        const wchar_t* logoUrl =
            L"https://raw.githubusercontent.com/personalworkcommissions-afk/"
            L"ShrimplockVersionUpdaters/main/logo%20%282%29.png";

        wchar_t tempPath[MAX_PATH]{};
        wchar_t tempFile[MAX_PATH]{};

        if (!GetTempPathW(MAX_PATH, tempPath))
            return false;

        if (!GetTempFileNameW(
            tempPath,
            L"SHR",
            0,
            tempFile
        ))
        {
            return false;
        }

        // Download logo from GitHub
        HRESULT downloadResult =
            URLDownloadToFileW(
                nullptr,
                logoUrl,
                tempFile,
                0,
                nullptr
            );

        if (FAILED(downloadResult))
        {
            DeleteFileW(tempFile);
            return false;
        }


        IWICImagingFactory* factory = nullptr;

        HRESULT hr =
            CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&factory)
            );

        if (FAILED(hr))
        {
            DeleteFileW(tempFile);
            return false;
        }


        IWICBitmapDecoder* decoder = nullptr;

        hr =
            factory->CreateDecoderFromFilename(
                tempFile,
                nullptr,
                GENERIC_READ,
                WICDecodeMetadataCacheOnLoad,
                &decoder
            );

        if (FAILED(hr))
        {
            factory->Release();
            DeleteFileW(tempFile);
            return false;
        }


        IWICBitmapFrameDecode* frame = nullptr;

        hr =
            decoder->GetFrame(
                0,
                &frame
            );

        if (FAILED(hr))
        {
            decoder->Release();
            factory->Release();
            DeleteFileW(tempFile);
            return false;
        }


        IWICFormatConverter* converter = nullptr;

        hr =
            factory->CreateFormatConverter(
                &converter
            );

        if (FAILED(hr))
        {
            frame->Release();
            decoder->Release();
            factory->Release();
            DeleteFileW(tempFile);
            return false;
        }


        hr =
            converter->Initialize(
                frame,
                GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0,
                WICBitmapPaletteTypeCustom
            );

        if (FAILED(hr))
        {
            converter->Release();
            frame->Release();
            decoder->Release();
            factory->Release();
            DeleteFileW(tempFile);
            return false;
        }


        UINT width = 0;
        UINT height = 0;

        converter->GetSize(
            &width,
            &height
        );

        UINT stride =
            width * 4;

        UINT imageSize =
            stride * height;


        std::vector<unsigned char> pixels(
            imageSize
        );


        hr =
            converter->CopyPixels(
                nullptr,
                stride,
                imageSize,
                pixels.data()
            );

        if (FAILED(hr))
        {
            converter->Release();
            frame->Release();
            decoder->Release();
            factory->Release();
            DeleteFileW(tempFile);
            return false;
        }


        D3D11_TEXTURE2D_DESC textureDesc{};

        textureDesc.Width = width;
        textureDesc.Height = height;

        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;

        textureDesc.Format =
            DXGI_FORMAT_R8G8B8A8_UNORM;

        textureDesc.SampleDesc.Count = 1;

        textureDesc.Usage =
            D3D11_USAGE_DEFAULT;

        textureDesc.BindFlags =
            D3D11_BIND_SHADER_RESOURCE;


        D3D11_SUBRESOURCE_DATA textureData{};

        textureData.pSysMem =
            pixels.data();

        textureData.SysMemPitch =
            stride;


        ID3D11Texture2D* texture = nullptr;

        hr =
            d3dDevice->CreateTexture2D(
                &textureDesc,
                &textureData,
                &texture
            );


        if (SUCCEEDED(hr))
        {
            hr =
                d3dDevice->CreateShaderResourceView(
                    texture,
                    nullptr,
                    &loadingLogoTexture
                );
        }


        if (texture)
            texture->Release();


        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();

        DeleteFileW(tempFile);


        return
            SUCCEEDED(hr) &&
            loadingLogoTexture != nullptr;
    }

    void SetupD3D11(HWND hwnd) {
        DXGI_SWAP_CHAIN_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = 2;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;

        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        D3D_FEATURE_LEVEL obtainedLevel;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            levels, 2, D3D11_SDK_VERSION, &sd,
            &swapChain, &d3dDevice, &obtainedLevel, &d3dContext
        );

        if (hr == DXGI_ERROR_UNSUPPORTED) {
            D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                levels, 2, D3D11_SDK_VERSION, &sd,
                &swapChain, &d3dDevice, &obtainedLevel, &d3dContext
            );
        }

        ID3D11Texture2D* backBuffer = nullptr;
        swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (backBuffer) {
            d3dDevice->CreateRenderTargetView(backBuffer, nullptr, &renderTarget);
            backBuffer->Release();
        }
    }

    void CleanupD3D11()
    {
        if (loadingLogoTexture)
        {
            loadingLogoTexture->Release();
            loadingLogoTexture = nullptr;
        }

        if (renderTarget)
        {
            renderTarget->Release();
            renderTarget = nullptr;
        }

        if (swapChain)
        {
            swapChain->Release();
            swapChain = nullptr;
        }

        if (d3dContext)
        {
            d3dContext->Release();
            d3dContext = nullptr;
        }

        if (d3dDevice)
        {
            d3dDevice->Release();
            d3dDevice = nullptr;
        }
    }

public:
    OverlayWindow() : windowHandle(nullptr), d3dDevice(nullptr), d3dContext(nullptr),
                      swapChain(nullptr), renderTarget(nullptr) {
        ZeroMemory(&windowClass, sizeof(windowClass));
    }

    std::string GetKeyName(int vKey) {
        if (vKey == 0) return "None";
        if (vKey == VK_LBUTTON) return "LMB";
        if (vKey == VK_RBUTTON) return "RMB";
        if (vKey == VK_MBUTTON) return "MMB";
        if (vKey == VK_XBUTTON1) return "MB4";
        if (vKey == VK_XBUTTON2) return "MB5";
        if (vKey == VK_SHIFT) return "Shift";
        if (vKey == VK_LSHIFT) return "LShift";
        if (vKey == VK_RSHIFT) return "RShift";
        if (vKey == VK_CONTROL) return "Ctrl";
        if (vKey == VK_LCONTROL) return "LCtrl";
        if (vKey == VK_RCONTROL) return "RCtrl";
        if (vKey == VK_MENU) return "Alt";
        if (vKey == VK_LMENU) return "LAlt";
        if (vKey == VK_RMENU) return "RAlt";
        if (vKey == VK_SPACE) return "Space";

        char name[128];
        int scanCode = MapVirtualKeyA(vKey, MAPVK_VK_TO_VSC);
        switch (vKey) {
            case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
            case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
            case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK:
                scanCode |= 0x100;
                break;
        }
        if (GetKeyNameTextA(scanCode << 16, name, 128) != 0) {
            std::string nStr = name;
            return nStr;
        }
        return "Key " + std::to_string(vKey);
    }

    bool IsFlingTarget(const std::string& name)
    {
        return std::find(
            Vars::Misc::flingTargets.begin(),
            Vars::Misc::flingTargets.end(),
            name
        ) != Vars::Misc::flingTargets.end();
    }

    void AddFlingTarget(const std::string& name)
    {
        if (!IsFlingTarget(name))
        {
            Vars::Misc::flingTargets.push_back(name);
        }
    }

    template <typename PlayerType>
    bool TeleportToPlayer(const PlayerType& plr)
    {
        auto localChar = Globals::localPlayer.GetModelRef();

        if (localChar.Addr == 0)
            return false;

        auto localHrp =
            localChar.FindCharacterPart("HumanoidRootPart");

        if (localHrp.Addr == 0)
            localHrp =
            localChar.FindCharacterPart("Root");

        if (localHrp.Addr == 0)
            return false;

        uintptr_t localPrim =
            localHrp.GetPrimitivePtr();

        if (localPrim == 0)
            return false;

        RBX::Vec3 targetPos = plr.position;

        targetPos.Y += 3.0f;

        Coms->WriteMemory<RBX::Vec3>(
            localPrim + Offsets::Primitive::Position,
            targetPos
        );

        Coms->WriteMemory<RBX::Vec3>(
            localPrim +
            Offsets::Primitive::AssemblyLinearVelocity,
            { 0.0f, 0.0f, 0.0f }
        );

        return true;
    }

    void RemoveFlingTarget(const std::string& name)
    {
        Vars::Misc::flingTargets.erase(
            std::remove(
                Vars::Misc::flingTargets.begin(),
                Vars::Misc::flingTargets.end(),
                name
            ),
            Vars::Misc::flingTargets.end()
        );
    }

    void StopFling()
    {
        if (!g_flingState.active)
            return;

        auto localChar =
            Globals::localPlayer.GetModelRef();

        if (localChar.Addr != 0)
        {
            auto root =
                localChar.FindCharacterPart(
                    "HumanoidRootPart"
                );

            if (root.Addr == 0)
                root =
                localChar.FindCharacterPart("Root");

            if (root.Addr != 0)
            {
                uintptr_t prim =
                    root.GetPrimitivePtr();

                if (prim != 0)
                {
                    Coms->WriteMemory<RBX::Vec3>(
                        prim +
                        Offsets::Primitive::AssemblyLinearVelocity,
                        { 0.0f, 0.0f, 0.0f }
                    );

                    Coms->WriteMemory<RBX::Vec3>(
                        prim +
                        Offsets::Primitive::Position,
                        g_flingState.oldPosition
                    );
                }
            }
        }

        g_flingState.active = false;
        g_flingState.targetName.clear();
    }

    template <typename PlayerType>
    void ToggleFling(const PlayerType& plr)
    {
        // If already flinging this player, stop properly
        if (g_flingState.active &&
            g_flingState.targetName == plr.name)
        {
            StopFling();
            return;
        }

        auto localChar =
            Globals::localPlayer.GetModelRef();

        if (localChar.Addr == 0)
            return;

        auto root =
            localChar.FindCharacterPart(
                "HumanoidRootPart"
            );

        if (root.Addr == 0)
            root =
            localChar.FindCharacterPart("Root");

        if (root.Addr == 0)
            return;

        uintptr_t prim =
            root.GetPrimitivePtr();

        if (prim == 0)
            return;

        g_flingState.oldPosition =
            Coms->ReadMemory<RBX::Vec3>(
                prim +
                Offsets::Primitive::Position
            );

        g_flingState.targetName =
            plr.name;

        g_flingState.startTime =
            std::chrono::steady_clock::now();

        g_flingState.active = true;
    }

    

    void UpdateFling()
    {
        if (!g_flingState.active)
            return;

        auto snapshot =
            PlayerCache::GetSnapshot();

        auto targetIt =
            std::find_if(
                snapshot.begin(),
                snapshot.end(),
                [](const auto& player)
                {
                    return
                        player.isValid &&
                        player.name ==
                        g_flingState.targetName;
                }
            );

        if (targetIt == snapshot.end())
        {
            g_flingState.active = false;
            return;
        }

        const auto& target = *targetIt;

        auto localChar =
            Globals::localPlayer.GetModelRef();

        if (localChar.Addr == 0)
        {
            g_flingState.active = false;
            return;
        }

        auto root =
            localChar.FindCharacterPart(
                "HumanoidRootPart"
            );

        if (root.Addr == 0)
            root =
            localChar.FindCharacterPart("Root");

        if (root.Addr == 0)
        {
            g_flingState.active = false;
            return;
        }

        uintptr_t prim =
            root.GetPrimitivePtr();

        if (prim == 0)
        {
            g_flingState.active = false;
            return;
        }

        float elapsed =
            std::chrono::duration<float>(
                std::chrono::steady_clock::now() -
                g_flingState.startTime
            ).count();

        RBX::Vec3 pos =
            target.position;

        // Move around the target continuously
        pos.X +=
            std::sin(elapsed * 70.0f) *
            1.5f;

        pos.Z +=
            std::cos(elapsed * 70.0f) *
            1.5f;

        Coms->WriteMemory<RBX::Vec3>(
            prim +
            Offsets::Primitive::Position,
            pos
        );

        Coms->WriteMemory<RBX::Vec3>(
            prim +
            Offsets::Primitive::AssemblyLinearVelocity,
            {
                120.0f,
                35.0f,
                120.0f
            }
        );
    }

    void StartFlingAll()
    {
        if (Vars::Misc::flingTargets.empty())
            return;

        if (Vars::Misc::flingAllActive)
            return;

        Vars::Misc::flingQueue =
            Vars::Misc::flingTargets;

        Vars::Misc::flingQueueIndex = 0;

        Vars::Misc::flingAllActive = true;
    }

    void UpdateFlingAll()
    {
        if (!Vars::Misc::flingAllActive)
            return;

        if (g_flingState.active)
            return;

        if (
            Vars::Misc::flingQueueIndex >=
            Vars::Misc::flingQueue.size()
            )
        {
            Vars::Misc::flingAllActive = false;
            Vars::Misc::flingQueue.clear();
            Vars::Misc::flingQueueIndex = 0;
            return;
        }

        std::string targetName =
            Vars::Misc::flingQueue[
                Vars::Misc::flingQueueIndex++
            ];

        auto snapshot =
            PlayerCache::GetSnapshot();

        auto targetIt =
            std::find_if(
                snapshot.begin(),
                snapshot.end(),
                [&](const auto& player)
                {
                    return
                        player.isValid &&
                        player.name == targetName;
                }
            );

        if (targetIt != snapshot.end())
        {
            ToggleFling(*targetIt);
        }
    }

    void RenderPlayerActionsWindow(
        const ImVec2& menuPos,
        const ImVec2& menuSize
    )
    {
        if (!Vars::Misc::playerActionsOpen)
            return;

        auto snapshot =
            PlayerCache::GetSnapshot();

        auto it =
            std::find_if(
                snapshot.begin(),
                snapshot.end(),
                [](const auto& player)
                {
                    return
                        player.isValid &&
                        player.name ==
                        Vars::Misc::actionTargetName;
                }
            );

        if (it == snapshot.end())
        {
            Vars::Misc::playerActionsOpen = false;
            Vars::Misc::actionTargetName.clear();
            return;
        }

        auto& plr = *it;

        ImGui::SetNextWindowPos(
            ImVec2(
                menuPos.x +
                menuSize.x +
                10.0f,

                menuPos.y
            ),
            ImGuiCond_Always
        );

        ImGui::SetNextWindowSize(
            ImVec2(230.0f, 280.0f),
            ImGuiCond_Always
        );

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowRounding,
            6.0f
        );

        ImGui::PushStyleColor(
            ImGuiCol_WindowBg,
            IM_COL32(12, 12, 12, 255)
        );

        ImGui::PushStyleColor(
            ImGuiCol_Border,
            IM_COL32(45, 45, 45, 255)
        );

        ImGui::Begin(
            "Player Actions",
            &Vars::Misc::playerActionsOpen,
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings
        );

        ImVec2 pos =
            ImGui::GetWindowPos();

        ImVec2 size =
            ImGui::GetWindowSize();

        Vars::Misc::actionsX = pos.x;
        Vars::Misc::actionsY = pos.y;
        Vars::Misc::actionsW = size.x;
        Vars::Misc::actionsH = size.y;

        ImGui::TextUnformatted(
            plr.displayName.c_str()
        );

        ImGui::TextDisabled(
            "@%s",
            plr.name.c_str()
        );

        ImGui::Separator();
        ImGui::Spacing();

        // ===============================================
        // SPECTATE
        // ===============================================

        bool isSpectating =
            Vars::Misc::spectating &&
            Vars::Misc::spectateTargetName ==
            plr.name;

        if (ImGui::Button(
            isSpectating
            ? "Stop Spectating"
            : "Spectate",
            ImVec2(-1.0f, 30.0f)
        ))
        {
            if (isSpectating)
            {
                Vars::Misc::spectating = false;

                Vars::Misc::spectateTargetName.clear();

                auto localChar =
                    Globals::localPlayer.GetModelRef();

                auto localHumanoid =
                    localChar.FindChildByClass(
                        "Humanoid"
                    );

                if (localHumanoid.Addr != 0)
                {
                    Coms->WriteMemory<uintptr_t>(
                        Globals::camera.Addr +
                        Offsets::Camera::CameraSubject,

                        localHumanoid.Addr
                    );
                }
            }
            else
            {
                Vars::Misc::spectating = true;

                Vars::Misc::spectateTargetName =
                    plr.name;

                if (plr.humanoidAddr != 0)
                {
                    Coms->WriteMemory<uintptr_t>(
                        Globals::camera.Addr +
                        Offsets::Camera::CameraSubject,

                        plr.humanoidAddr
                    );
                }
            }
        }

        // ===============================================
        // TELEPORT
        // ===============================================

        if (ImGui::Button(
            "Teleport",
            ImVec2(-1.0f, 30.0f)
        ))
        {
            TeleportToPlayer(plr);
        }

        // ===============================================
        // WHITELIST
        // ===============================================

        bool whitelisted =
            std::find(
                Vars::Misc::whitelistedPlayers.begin(),
                Vars::Misc::whitelistedPlayers.end(),
                plr.name
            )
            != Vars::Misc::whitelistedPlayers.end();

        if (ImGui::Button(
            whitelisted
            ? "Unwhitelist"
            : "Whitelist",
            ImVec2(-1.0f, 30.0f)
        ))
        {
            if (whitelisted)
            {
                Vars::Misc::whitelistedPlayers.erase(
                    std::remove(
                        Vars::Misc::whitelistedPlayers.begin(),
                        Vars::Misc::whitelistedPlayers.end(),
                        plr.name
                    ),
                    Vars::Misc::whitelistedPlayers.end()
                );
            }
            else
            {
                Vars::Misc::whitelistedPlayers.push_back(
                    plr.name
                );
            }
        }

        // ===============================================
// ENEMY LIST
// ===============================================

        bool isEnemy =
            std::find(
                Vars::Misc::enemyPlayers.begin(),
                Vars::Misc::enemyPlayers.end(),
                plr.name
            ) != Vars::Misc::enemyPlayers.end();

        if (ImGui::Button(
            isEnemy
            ? "Remove from Enemy List"
            : "Add to Enemy List",
            ImVec2(-1.0f, 30.0f)
        ))
        {
            if (isEnemy)
            {
                Vars::Misc::enemyPlayers.erase(
                    std::remove(
                        Vars::Misc::enemyPlayers.begin(),
                        Vars::Misc::enemyPlayers.end(),
                        plr.name
                    ),
                    Vars::Misc::enemyPlayers.end()
                );
            }
            else
            {
                // Can't be whitelisted and an enemy at the same time
                Vars::Misc::whitelistedPlayers.erase(
                    std::remove(
                        Vars::Misc::whitelistedPlayers.begin(),
                        Vars::Misc::whitelistedPlayers.end(),
                        plr.name
                    ),
                    Vars::Misc::whitelistedPlayers.end()
                );

                Vars::Misc::enemyPlayers.push_back(plr.name);
            }
        }

        // ===============================================
        // ADD / REMOVE FLING TARGET
        // ===============================================

        bool targeted =
            IsFlingTarget(plr.name);

        if (ImGui::Button(
            targeted
            ? "Remove Fling Target"
            : "Add Fling Target",
            ImVec2(-1.0f, 30.0f)
        ))
        {
            if (targeted)
                RemoveFlingTarget(plr.name);
            else
                AddFlingTarget(plr.name);
        }

        // ===============================================
        // FLING NOW
        // ===============================================

        bool flingingThis =
            g_flingState.active &&
            g_flingState.targetName == plr.name;

        if (ImGui::Button(
            flingingThis
            ? "Stop Fling"
            : "Fling",
            ImVec2(-1.0f, 30.0f)
        ))
        {
            ToggleFling(plr);
        }

        ImGui::End();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
    }

    void RenderFlingTargetsWindow()
    {
        if (!Vars::Misc::flingTargetsWindowOpen)
            return;

        ImGui::SetNextWindowSize(
            ImVec2(300.0f, 300.0f),
            ImGuiCond_FirstUseEver
        );

        ImGui::SetNextWindowPos(
            ImVec2(300.0f, 180.0f),
            ImGuiCond_FirstUseEver
        );

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowRounding,
            6.0f
        );

        ImGui::PushStyleColor(
            ImGuiCol_WindowBg,
            IM_COL32(12, 12, 12, 255)
        );

        ImGui::PushStyleColor(
            ImGuiCol_Border,
            IM_COL32(45, 45, 45, 255)
        );

        ImGui::Begin(
            "Fling Targets",
            &Vars::Misc::flingTargetsWindowOpen,
            ImGuiWindowFlags_NoSavedSettings
        );

        ImVec2 pos =
            ImGui::GetWindowPos();

        ImVec2 size =
            ImGui::GetWindowSize();

        Vars::Misc::flingWindowX = pos.x;
        Vars::Misc::flingWindowY = pos.y;
        Vars::Misc::flingWindowW = size.x;
        Vars::Misc::flingWindowH = size.y;

        ImGui::TextDisabled(
            "%d Targeted Player%s",
            static_cast<int>(
                Vars::Misc::flingTargets.size()
                ),
            Vars::Misc::flingTargets.size() == 1
            ? ""
            : "s"
        );

        ImGui::Separator();
        ImGui::Spacing();

        if (Vars::Misc::flingTargets.empty())
        {
            ImGui::TextDisabled(
                "No targeted players."
            );
        }
        else
        {
            auto targets =
                Vars::Misc::flingTargets;

            for (const auto& name : targets)
            {
                ImGui::PushID(name.c_str());

                ImGui::TextUnformatted(
                    name.c_str()
                );

                ImGui::SameLine();

                ImGui::SetCursorPosX(
                    ImGui::GetWindowWidth() -
                    80.0f
                );

                if (ImGui::Button(
                    "Remove",
                    ImVec2(65.0f, 23.0f)
                ))
                {
                    RemoveFlingTarget(name);
                }

                ImGui::Separator();

                ImGui::PopID();
            }
        }

        ImGui::Spacing();

        bool empty =
            Vars::Misc::flingTargets.empty();

        if (empty)
            ImGui::BeginDisabled();

        if (ImGui::Button(
            Vars::Misc::flingAllActive
            ? "Flinging..."
            : "Fling All",
            ImVec2(-1.0f, 30.0f)
        ))
        {
            StartFlingAll();
        }

        if (empty)
            ImGui::EndDisabled();

        ImGui::End();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
    }

    void RenderEnemyListWindow()
    {
        if (!Vars::Misc::enemyListWindowOpen)
            return;

        ImGui::SetNextWindowSize(
            ImVec2(300.0f, 300.0f),
            ImGuiCond_FirstUseEver
        );

        ImGui::SetNextWindowPos(
            ImVec2(300.0f, 180.0f),
            ImGuiCond_FirstUseEver
        );

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowRounding,
            6.0f
        );

        ImGui::PushStyleColor(
            ImGuiCol_WindowBg,
            IM_COL32(12, 12, 12, 255)
        );

        ImGui::PushStyleColor(
            ImGuiCol_Border,
            IM_COL32(45, 45, 45, 255)
        );

        ImGui::Begin(
            "Enemy List",
            &Vars::Misc::enemyListWindowOpen,
            ImGuiWindowFlags_NoSavedSettings
        );

        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();

        Vars::Misc::enemyWindowX = pos.x;
        Vars::Misc::enemyWindowY = pos.y;
        Vars::Misc::enemyWindowW = size.x;
        Vars::Misc::enemyWindowH = size.y;

        ImGui::TextDisabled(
            "%d Enem%s",
            static_cast<int>(Vars::Misc::enemyPlayers.size()),
            Vars::Misc::enemyPlayers.size() == 1 ? "y" : "ies"
        );

        ImGui::Separator();
        ImGui::Spacing();

        if (Vars::Misc::enemyPlayers.empty())
        {
            ImGui::TextDisabled("No enemies added.");
        }
        else
        {
            auto enemies = Vars::Misc::enemyPlayers;

            for (const auto& name : enemies)
            {
                ImGui::PushID(name.c_str());

                ImGui::TextUnformatted(name.c_str());

                ImGui::SameLine();

                ImGui::SetCursorPosX(
                    ImGui::GetWindowWidth() - 80.0f
                );

                if (ImGui::Button(
                    "Remove",
                    ImVec2(65.0f, 23.0f)
                ))
                {
                    Vars::Misc::enemyPlayers.erase(
                        std::remove(
                            Vars::Misc::enemyPlayers.begin(),
                            Vars::Misc::enemyPlayers.end(),
                            name
                        ),
                        Vars::Misc::enemyPlayers.end()
                    );
                }

                ImGui::Separator();

                ImGui::PopID();
            }
        }

        ImGui::End();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
    }

    inline ImVec4 ServerAccent(float alpha = 1.0f)
    {
        return ImVec4(
            Vars::Settings::themeColor[0],
            Vars::Settings::themeColor[1],
            Vars::Settings::themeColor[2],
            alpha
        );
    }

    inline ImVec4 ServerMix(const ImVec4& a, const ImVec4& b, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return ImVec4(
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
            a.w + (b.w - a.w) * t
        );
    }

    void RenderServersWindow()
    {
        if (!Vars::Servers::menuOpen)
            return;

        ImVec4 accent = ServerAccent(1.0f);
        ImVec4 accentSoft = ServerAccent(0.18f);
        ImVec4 accentSofter = ServerAccent(0.10f);

        ImVec4 windowBg = ImVec4(0.025f, 0.026f, 0.030f, 1.0f);
        ImVec4 panelBg = ImVec4(0.038f, 0.039f, 0.045f, 1.0f);
        ImVec4 panelBg2 = ImVec4(0.050f, 0.051f, 0.060f, 1.0f);

        ImVec4 borderCol = ServerMix(windowBg, accent, 0.28f);
        ImVec4 buttonCol = ServerMix(panelBg, accent, 0.20f);
        ImVec4 buttonHover = ServerMix(panelBg2, accent, 0.40f);
        ImVec4 buttonActive = ServerMix(panelBg2, accent, 0.65f);
        ImVec4 frameBg = ImVec4(0.060f, 0.061f, 0.070f, 1.0f);
        ImVec4 frameHover = ServerMix(frameBg, accent, 0.30f);
        ImVec4 frameActive = ServerMix(frameBg, accent, 0.50f);

        ImGui::SetNextWindowSize(
            ImVec2(500.0f, 400.0f),
            ImGuiCond_FirstUseEver
        );

        ImGui::SetNextWindowPos(
            ImVec2(300.0f, 180.0f),
            ImGuiCond_FirstUseEver
        );


        // ============================================================
        // SERVER STYLE
        // ============================================================

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowPadding,
            ImVec2(12, 10)
        );

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowRounding,
            6.0f
        );

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowBorderSize,
            1.0f
        );

        ImGui::PushStyleColor(
            ImGuiCol_WindowBg,
            windowBg
        );

        ImGui::PushStyleColor(
            ImGuiCol_Border,
            borderCol
        );

        ImGui::PushStyleColor(
            ImGuiCol_FrameBg,
            frameBg
        );

        ImGui::PushStyleColor(
            ImGuiCol_FrameBgHovered,
            frameHover
        );

        ImGui::PushStyleColor(
            ImGuiCol_FrameBgActive,
            frameActive
        );

        ImGui::PushStyleColor(
            ImGuiCol_Button,
            buttonCol
        );

        ImGui::PushStyleColor(
            ImGuiCol_ButtonHovered,
            buttonHover
        );

        ImGui::PushStyleColor(
            ImGuiCol_ButtonActive,
            buttonActive
        );

        ImGui::PushStyleColor(
            ImGuiCol_Header,
            accentSoft
        );

        ImGui::PushStyleColor(
            ImGuiCol_HeaderHovered,
            ImVec4(
                accent.x,
                accent.y,
                accent.z,
                0.30f
            )
        );

        ImGui::PushStyleColor(
            ImGuiCol_HeaderActive,
            ImVec4(
                accent.x,
                accent.y,
                accent.z,
                0.45f
            )
        );

        ImGui::PushStyleColor(
            ImGuiCol_CheckMark,
            accent
        );

        ImGui::PushStyleColor(
            ImGuiCol_SliderGrab,
            accent
        );

        ImGui::PushStyleColor(
            ImGuiCol_SliderGrabActive,
            accent
        );

        ImGui::Begin(
            "Servers",
            &Vars::Servers::menuOpen,
            ImGuiWindowFlags_NoSavedSettings
        );

        // Save window bounds for click-through handling
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();

        Vars::Servers::windowX = pos.x;
        Vars::Servers::windowY = pos.y;
        Vars::Servers::windowW = size.x;
        Vars::Servers::windowH = size.y;

        // ============================================================
        // HEADER
        // ============================================================

        ImGui::TextColored(accent, "Servers");
        ImGui::SameLine();
        ImGui::TextDisabled("Public Servers");

        ImGui::SameLine();

        const char* sortNames[] =
        {
            "Lowest Ping",
            "Most Players",
            "Least Players"
        };

        int currentSort =
            static_cast<int>(ServerAPI::sortMode);

        ImGui::SetCursorPosX(
            ImGui::GetWindowWidth() - 140.0f
        );

        ImGui::SetNextItemWidth(125.0f);

        if (ImGui::Combo(
            "##ServerSort",
            &currentSort,
            sortNames,
            IM_ARRAYSIZE(sortNames)))
        {
            ServerAPI::sortMode =
                static_cast<ServerAPI::SortMode>(currentSort);

            ServerAPI::Sort();
        }

        ImGui::Separator();
        ImGui::Spacing();

        // ============================================================
        // STATUS
        // ============================================================

        if (ServerAPI::loading)
        {
            ImGui::TextDisabled("Loading public servers...");
            ImGui::Spacing();
        }
        else if (!ServerAPI::errorMessage.empty())
        {
            ImGui::TextDisabled(
                "%s",
                ServerAPI::errorMessage.c_str()
            );

            ImGui::Spacing();
        }

        // ============================================================
        // COPY SERVER LIST
        // ============================================================

        std::vector<ServerAPI::Server> serverCopy;

        {
            std::lock_guard<std::mutex> lock(
                ServerAPI::serverMutex
            );

            serverCopy = ServerAPI::servers;
        }

        // ============================================================
        // LIST
        // ============================================================

        ImGui::BeginChild(
            "##ServerList",
            ImVec2(
                0.0f,
                ImGui::GetContentRegionAvail().y - 42.0f
            ),
            false
        );

        if (!ServerAPI::loading &&
            serverCopy.empty() &&
            ServerAPI::errorMessage.empty())
        {
            ImGui::TextDisabled("No servers loaded.");
        }

        for (size_t i = 0; i < serverCopy.size(); ++i)
        {
            const ServerAPI::Server& server =
                serverCopy[i];

            ImGui::PushID(
                static_cast<int>(i)
            );

            // ============================================================
// SERVER CARD
// ============================================================

            ImVec2 rowStart = ImGui::GetCursorScreenPos();

            float rowWidth =
                ImGui::GetContentRegionAvail().x;

            float rowHeight = 54.0f;

            ImVec2 rowEnd(
                rowStart.x + rowWidth,
                rowStart.y + rowHeight
            );

            bool hovered =
                ImGui::IsMouseHoveringRect(
                    rowStart,
                    rowEnd
                );

            ImDrawList* draw =
                ImGui::GetWindowDrawList();


            // ============================================================
            // ROW BACKGROUND
            // ============================================================

            ImU32 rowBg =
                ImGui::GetColorU32(
                    hovered
                    ? panelBg2
                    : panelBg
                );

            draw->AddRectFilled(
                rowStart,
                rowEnd,
                rowBg,
                5.0f
            );


            // ============================================================
            // THEME ACCENT STRIP
            // ============================================================

            if (hovered)
            {
                draw->AddRectFilled(
                    rowStart,
                    ImVec2(
                        rowStart.x + 3.0f,
                        rowEnd.y
                    ),
                    ImGui::GetColorU32(accent),
                    5.0f
                );
            }


            // ============================================================
            // PLAYER COUNT
            // ============================================================

            char playerText[64];

            sprintf_s(
                playerText,
                "%d / %d players",
                server.playing,
                server.maxPlayers
            );

            draw->AddText(
                ImVec2(
                    rowStart.x + 12.0f,
                    rowStart.y + 8.0f
                ),
                ImGui::GetColorU32(ImGuiCol_Text),
                playerText
            );


            // ============================================================
            // PLAYER BAR
            // ============================================================

            float ratio =
                server.maxPlayers > 0
                ? static_cast<float>(server.playing) /
                static_cast<float>(server.maxPlayers)
                : 0.0f;

            ratio = std::clamp(
                ratio,
                0.0f,
                1.0f
            );

            ImVec2 barMin(
                rowStart.x + 12.0f,
                rowStart.y + 31.0f
            );

            ImVec2 barMax(
                rowStart.x + 170.0f,
                rowStart.y + 36.0f
            );

            draw->AddRectFilled(
                barMin,
                barMax,
                IM_COL32(40, 40, 46, 255),
                3.0f
            );

            draw->AddRectFilled(
                barMin,
                ImVec2(
                    barMin.x +
                    (barMax.x - barMin.x) * ratio,
                    barMax.y
                ),
                ImGui::GetColorU32(accent),
                3.0f
            );


            // ============================================================
            // PING COLOR
            // ============================================================

            ImVec4 pingColor;

            if (server.ping <= 50)
            {
                pingColor =
                    ImVec4(0.35f, 0.90f, 0.35f, 1.0f);
            }
            else if (server.ping <= 100)
            {
                pingColor =
                    ImVec4(0.95f, 0.80f, 0.25f, 1.0f);
            }
            else
            {
                pingColor =
                    ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
            }


            // ============================================================
            // PING TEXT
            // ============================================================

            char pingText[32];

            sprintf_s(
                pingText,
                "%d ms",
                server.ping
            );

            draw->AddText(
                ImVec2(
                    rowStart.x + 195.0f,
                    rowStart.y + 18.0f
                ),
                ImGui::GetColorU32(pingColor),
                pingText
            );


            // ============================================================
            // FPS
            // ============================================================

            char fpsText[32];

            sprintf_s(
                fpsText,
                "%d FPS",
                server.fps
            );

            draw->AddText(
                ImVec2(
                    rowStart.x + 265.0f,
                    rowStart.y + 18.0f
                ),
                ImGui::GetColorU32(ImGuiCol_TextDisabled),
                fpsText
            );


            // ============================================================
            // JOIN BUTTON
            // ============================================================

            ImGui::SetCursorScreenPos(
                ImVec2(
                    rowEnd.x - 78.0f,
                    rowStart.y + 13.0f
                )
            );

            if (ImGui::Button(
                "Join",
                ImVec2(66.0f, 28.0f)))
            {
                ServerAPI::Join(
                    ServerAPI::currentPlaceId,
                    server.id
                );
            }

            // ============================================================
// RESERVE CARD SPACE / ADVANCE TO NEXT ROW
// ============================================================

            ImGui::SetCursorScreenPos(rowStart);

            ImGui::Dummy(
                ImVec2(
                    rowWidth,
                    rowHeight + 6.0f
                )
            );

            ImGui::PopID();
        }

        ImGui::EndChild();

        // ============================================================
        // REFRESH
        // ============================================================

        const bool wasLoading = ServerAPI::loading;

        if (wasLoading)
            ImGui::BeginDisabled();

        if (ImGui::Button(
            wasLoading
            ? "Refreshing..."
            : "Refresh Servers",
            ImVec2(-1.0f, 30.0f)))
        {
            uint64_t placeId =
                Globals::dataModel.GetPlaceId();

            if (placeId == 0)
            {
                ServerAPI::errorMessage =
                    "Unable to determine current PlaceId.";
            }
            else
            {
                ServerAPI::Refresh(placeId);
            }
        }

        if (wasLoading)
            ImGui::EndDisabled();

        ImGui::End();

        ImGui::PopStyleColor(14);
        ImGui::PopStyleVar(3);
    }

    void RenderLoadingScreen(float progress, const char* status)
    {
        if (!loadingAnimationStarted)
        {
            loadingAnimationStarted = true;

            loadingAnimationStart =
                std::chrono::steady_clock::now();
        }


        float elapsed =
            std::chrono::duration<float>(
                std::chrono::steady_clock::now() -
                loadingAnimationStart
            ).count();


        // ============================================
        // FADE IN
        // ============================================

        float fade =
            std::clamp(
                elapsed / 1.35f,
                0.0f,
                1.0f
            );


        // Smoothstep
        fade =
            fade * fade *
            (3.0f - 2.0f * fade);


        // ============================================
        // ZOOM
        // ============================================

        // Starts smaller, slowly reaches normal size
        float logoSize =
            155.0f +
            (85.0f * fade);


        BeginFrame();


        ImDrawList* drawList =
            ImGui::GetBackgroundDrawList();


        ImVec2 displaySize =
            ImGui::GetIO().DisplaySize;


        // ============================================
        // LOGO
        // ============================================

        if (loadingLogoTexture)
        {
            ImVec2 logoMin = ImVec2(
                (displaySize.x - logoSize) * 0.5f,
                (displaySize.y - logoSize) * 0.5f
            );

            ImVec2 logoMax = ImVec2(
                logoMin.x + logoSize,
                logoMin.y + logoSize
            );


            int alpha =
                static_cast<int>(
                    fade * 255.0f
                    );


            drawList->AddImage(
                loadingLogoTexture,
                logoMin,
                logoMax,

                ImVec2(
                    0.0f,
                    0.0f
                ),

                ImVec2(
                    1.0f,
                    1.0f
                ),

                IM_COL32(
                    255,
                    255,
                    255,
                    alpha
                )
            );
        }


        EndFrame();
    }

    void FadeOutLoadingScreen()
    {
        const float fadeDuration =
            0.85f;


        auto start =
            std::chrono::steady_clock::now();


        while (true)
        {
            float elapsed =
                std::chrono::duration<float>(
                    std::chrono::steady_clock::now() -
                    start
                ).count();


            float t =
                std::clamp(
                    elapsed / fadeDuration,
                    0.0f,
                    1.0f
                );


            float smooth =
                t * t *
                (3.0f - 2.0f * t);


            float alpha =
                1.0f - smooth;


            // Slight extra zoom while disappearing
            float logoSize =
                240.0f +
                (30.0f * smooth);


            BeginFrame();


            ImDrawList* drawList =
                ImGui::GetBackgroundDrawList();


            ImVec2 displaySize =
                ImGui::GetIO().DisplaySize;


            int alpha255 =
                static_cast<int>(
                    alpha * 255.0f
                    );

            if (loadingLogoTexture)
            {
                ImVec2 logoMin = ImVec2(
                    (displaySize.x - logoSize) * 0.5f,
                    (displaySize.y - logoSize) * 0.5f
                );

                ImVec2 logoMax = ImVec2(
                    logoMin.x + logoSize,
                    logoMin.y + logoSize
                );


                drawList->AddImage(
                    loadingLogoTexture,
                    logoMin,
                    logoMax,

                    ImVec2(
                        0.0f,
                        0.0f
                    ),

                    ImVec2(
                        1.0f,
                        1.0f
                    ),

                    IM_COL32(
                        255,
                        255,
                        255,
                        alpha255
                    )
                );
            }


            EndFrame();


            if (t >= 1.0f)
                break;


            std::this_thread::sleep_for(
                std::chrono::milliseconds(8)
            );
        }
    }

    void RenderUtilityBar()
    {
        if (!Vars::menuOpen)
            return;

        ImGuiIO& io = ImGui::GetIO();

        const ImVec2 barSize(400.0f, 46.0f);

        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, 18.0f),
            ImGuiCond_FirstUseEver,
            ImVec2(0.5f, 0.0f)
        );

        ImGui::SetNextWindowSize(barSize, ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(12, 12, 12, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(45, 45, 45, 255));

        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(20, 20, 21, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(30, 30, 31, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(38, 38, 39, 255));

        ImGui::Begin(
            "##ShrimplockUtilityBar",
            nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings
        );

        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();

        Vars::Utility::toolbarX = pos.x;
        Vars::Utility::toolbarY = pos.y;
        Vars::Utility::toolbarW = size.x;
        Vars::Utility::toolbarH = size.y;

        const ImVec2 buttonSize(38.0f, 32.0f);

        // =====================================================
        // EXPLORER
        // =====================================================

        if (ImGui::Button("E##UtilityExplorer", buttonSize))
        {
            Vars::Explorer::enabled = !Vars::Explorer::enabled;
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Explorer");

        ImGui::SameLine();

        // =====================================================
// REMOTE SPY
// =====================================================

        if (ImGui::Button(
            "RS##UtilityRemoteSpy",
            buttonSize
        ))
        {
            Vars::RemoteSpy::enabled =
                !Vars::RemoteSpy::enabled;
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Remote Spy"
            );
        }

        ImGui::SameLine();

        // =====================================================
        // KEYBINDS
        // =====================================================

        if (ImGui::Button("K##UtilityKeybinds", buttonSize))
        {
            Vars::Utility::keybindsOpen =
                !Vars::Utility::keybindsOpen;
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Keybinds");

        ImGui::SameLine();

        // =====================================================
        // LUA EDITOR
        // =====================================================

        if (ImGui::Button("<>##UtilityLua", buttonSize))
        {
            Vars::Utility::luaEditorOpen =
                !Vars::Utility::luaEditorOpen;
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Lua Code Editor");

        ImGui::SameLine();

        if (ImGui::Button("M##UtilityMusic", buttonSize))
        {
            Vars::Music::menuOpen =
                !Vars::Music::menuOpen;
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Music");

        ImGui::SameLine();

        if (ImGui::Button("SV##UtilityServers", buttonSize))
        {
            Vars::Servers::menuOpen =
                !Vars::Servers::menuOpen;
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Servers");

        ImGui::SameLine();

        if (ImGui::Button(
            "T##UtilityFlingTargets",
            buttonSize
        ))
        {
            Vars::Misc::flingTargetsWindowOpen =
                !Vars::Misc::flingTargetsWindowOpen;
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Fling Targets (%d)",
                static_cast<int>(
                    Vars::Misc::flingTargets.size()
                    )
            );
        }

        ImGui::SameLine();

        if (ImGui::Button(
            "EN##UtilityEnemyList",
            buttonSize
        ))
        {
            Vars::Misc::enemyListWindowOpen =
                !Vars::Misc::enemyListWindowOpen;
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Enemy List (%d)",
                static_cast<int>(
                    Vars::Misc::enemyPlayers.size()
                    )
            );
        }

        ImGui::End();

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(3);
    }

    void RenderMusicWindow()
    {
        if (!Vars::Music::menuOpen)
            return;

        ImGui::SetNextWindowSize(
            ImVec2(420.0f, 430.0f),
            ImGuiCond_FirstUseEver
        );

        ImGui::SetNextWindowPos(
            ImVec2(300.0f, 180.0f),
            ImGuiCond_FirstUseEver
        );

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowPadding,
            ImVec2(12, 10)
        );

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowRounding,
            6.0f
        );

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowBorderSize,
            1.0f
        );

        ImGui::PushStyleColor(
            ImGuiCol_WindowBg,
            IM_COL32(12, 12, 12, 255)
        );

        ImGui::PushStyleColor(
            ImGuiCol_Border,
            IM_COL32(45, 45, 45, 255)
        );

        ImGui::Begin(
            "Music",
            &Vars::Music::menuOpen,
            ImGuiWindowFlags_NoSavedSettings
        );

        ImVec2 pos =
            ImGui::GetWindowPos();

        ImVec2 size =
            ImGui::GetWindowSize();

        Vars::Music::windowX = pos.x;
        Vars::Music::windowY = pos.y;
        Vars::Music::windowW = size.x;
        Vars::Music::windowH = size.y;


        // ============================================================
        // SEARCH
        // ============================================================

        ImGui::TextUnformatted("Audius Music");

        ImGui::Spacing();

        static char searchBuffer[128] = "";

        ImGui::SetNextItemWidth(
            ImGui::GetContentRegionAvail().x - 85.0f
        );

        bool enterPressed =
            ImGui::InputText(
                "##MusicSearch",
                searchBuffer,
                sizeof(searchBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue
            );

        ImGui::SameLine();

        bool searchPressed =
            ImGui::Button(
                "Search",
                ImVec2(75.0f, 0.0f)
            );

        if (enterPressed || searchPressed)
        {
            Music::searchQuery =
                searchBuffer;

            if (Music::searchQuery.empty())
            {
                Music::statusText =
                    "Enter a song or artist.";
            }
            else
            {
                // Actual Audius Search() gets connected later.
                Music::loading = true;
                Music::statusText =
                    "Searching...";

                auto results =
                    AudiusAPI::Search(
                        Music::searchQuery
                    );

                Music::SetSearchResults(
                    results
                );

                Music::loading = false;

                if (results.empty())
                {
                    Music::statusText =
                        "No results found.";
                }
                else
                {
                    Music::statusText =
                        "Found " +
                        std::to_string(results.size()) +
                        " tracks.";
                }
            }
        }


        ImGui::Spacing();

        ImGui::TextDisabled(
            "%s",
            Music::statusText.c_str()
        );

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();


        // ============================================================
        // SEARCH RESULTS
        // ============================================================

        ImGui::TextUnformatted("Results");

        ImGui::Spacing();

        auto results =
            Music::GetSearchResults();

        ImGui::BeginChild(
            "##MusicResults",
            ImVec2(
                0.0f,
                160.0f
            ),
            true
        );

        if (results.empty())
        {
            ImGui::TextDisabled(
                "No search results."
            );
        }

        for (size_t i = 0;
            i < results.size();
            ++i)
        {
            const auto& track =
                results[i];

            ImGui::PushID(
                static_cast<int>(i)
            );

            ImGui::TextUnformatted(
                track.title.c_str()
            );

            ImGui::TextDisabled(
                "%s",
                track.artist.c_str()
            );

            ImGui::SameLine();

            ImGui::SetCursorPosX(
                ImGui::GetWindowWidth() -
                75.0f
            );

            if (ImGui::Button(
                "Select",
                ImVec2(60.0f, 24.0f)
            ))
            {
                Music::loading = true;

                Music::statusText =
                    "Loading " +
                    track.title +
                    "...";

                std::filesystem::path cachedFile;

                if (
                    AudiusAPI::DownloadTrack(
                        track,
                        cachedFile
                    )
                    )
                {
                    if (
                        MusicPlayer::LoadFile(
                            track,
                            cachedFile.string()
                        )
                        )
                    {
                        MusicPlayer::Play();

                        Music::statusText =
                            "Playing " +
                            track.title;
                    }
                    else
                    {
                        Music::statusText =
                            "Failed to load audio.";
                    }
                }
                else
                {
                    Music::statusText =
                        "Failed to download stream.";
                }

                Music::loading = false;

                Music::statusText =
                    "Selected: " +
                    track.title;
            }

            ImGui::Separator();

            ImGui::PopID();
        }

        ImGui::EndChild();


        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();


        // ============================================================
        // CURRENT TRACK
        // ============================================================

        if (Music::hasCurrentTrack)
        {
            auto track =
                Music::GetCurrentTrack();

            ImGui::TextUnformatted(
                track.title.c_str()
            );

            ImGui::TextDisabled(
                "%s",
                track.artist.c_str()
            );

            ImGui::Spacing();


            // ========================================================
            // PLAYBACK BUTTONS
            // ========================================================

            const float buttonWidth =
                82.0f;

            if (ImGui::Button(
                "<<##MusicPrevious",
                ImVec2(buttonWidth, 30.0f)
            ))
            {
                MusicPlayer::Previous();
            }

            ImGui::SameLine();

            if (MusicPlayer::IsPlaying())
            {
                if (ImGui::Button(
                    "Pause",
                    ImVec2(buttonWidth, 30.0f)
                ))
                {
                    MusicPlayer::Pause();
                }
            }
            else
            {
                const char* text =
                    MusicPlayer::IsPaused()
                    ? "Resume"
                    : "Play";

                if (ImGui::Button(
                    text,
                    ImVec2(buttonWidth, 30.0f)
                ))
                {
                    if (MusicPlayer::IsPaused())
                    {
                        MusicPlayer::Resume();
                    }
                    else
                    {
                        MusicPlayer::Play();
                    }
                }
            }

            ImGui::SameLine();

            if (ImGui::Button(
                ">>##MusicNext",
                ImVec2(buttonWidth, 30.0f)
            ))
            {
                MusicPlayer::Next();
            }

            ImGui::SameLine();

            if (ImGui::Button(
                "Stop",
                ImVec2(70.0f, 30.0f)
            ))
            {
                MusicPlayer::Stop();
            }


            // ========================================================
            // PROGRESS
            // ========================================================

            ImGui::Spacing();

            float current =
                MusicPlayer::GetPosition();

            float duration =
                MusicPlayer::GetDuration();

            if (duration <= 0.0f)
                duration =
                Music::durationSeconds;

            if (duration > 0.0f)
            {
                float progress =
                    current / duration;

                if (progress < 0.0f)
                    progress = 0.0f;

                if (progress > 1.0f)
                    progress = 1.0f;

                ImGui::ProgressBar(
                    progress,
                    ImVec2(-1.0f, 8.0f)
                );

                int currentMinutes =
                    static_cast<int>(current) / 60;

                int currentSeconds =
                    static_cast<int>(current) % 60;

                int durationMinutes =
                    static_cast<int>(duration) / 60;

                int durationSeconds =
                    static_cast<int>(duration) % 60;

                ImGui::TextDisabled(
                    "%d:%02d / %d:%02d",
                    currentMinutes,
                    currentSeconds,
                    durationMinutes,
                    durationSeconds
                );
            }


            // ========================================================
            // VOLUME
            // ========================================================

            ImGui::Spacing();

            float volume =
                Music::GetVolume();

            float volumePercent =
                volume * 100.0f;

            if (ImGui::SliderFloat(
                "Volume",
                &volumePercent,
                0.0f,
                100.0f,
                "%.0f%%",
                ImGuiSliderFlags_AlwaysClamp
            ))
            {
                MusicPlayer::SetVolume(
                    volumePercent / 100.0f
                );
            }
        }
        else
        {
            ImGui::TextDisabled(
                "No track selected."
            );
        }


        ImGui::End();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }

    void RenderKeybindWindow()
    {
        if (!Vars::Utility::keybindsOpen)
            return;

        ImGui::SetNextWindowSize(
            ImVec2(310.0f, 0.0f),
            ImGuiCond_FirstUseEver
        );

        ImGui::SetNextWindowPos(
            ImVec2(30.0f, 180.0f),
            ImGuiCond_FirstUseEver
        );

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

        ImGui::PushStyleColor(
            ImGuiCol_WindowBg,
            IM_COL32(12, 12, 12, 255)
        );

        ImGui::PushStyleColor(
            ImGuiCol_Border,
            IM_COL32(45, 45, 45, 255)
        );

        ImGui::Begin(
            "##HotkeyStatus",
            nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings
        );

        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();

        Vars::Utility::keybindX = pos.x;
        Vars::Utility::keybindY = pos.y;
        Vars::Utility::keybindW = size.x;
        Vars::Utility::keybindH = size.y;

        // ============================================================
        // HEADER
        // ============================================================

        ImGui::TextUnformatted("Hotkeys");

        ImGui::SameLine();

        float buttonW = 55.0f;

        ImGui::SetCursorPosX(
            ImGui::GetWindowWidth() -
            buttonW -
            ImGui::GetStyle().WindowPadding.x
        );

        if (ImGui::Button(
            Vars::Utility::showHotkeys ? "Hide" : "Show",
            ImVec2(buttonW, 22)
        ))
        {
            Vars::Utility::showHotkeys =
                !Vars::Utility::showHotkeys;
        }

        // ============================================================
        // HOTKEY LIST
        // ============================================================

        if (Vars::Utility::showHotkeys)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            auto HotkeyRow =
                [&](const char* name, int* key)
                {
                    // Don't show completely unbound hotkeys
                    // Remove this if you want "None" rows visible too.
                    if (!key)
                        return;

                    ImGui::PushID(key);

                    float startY = ImGui::GetCursorPosY();

                    // ----------------------------------------
                    // Name
                    // ----------------------------------------

                    ImGui::TextUnformatted(name);

                    // ----------------------------------------
                    // Key name
                    // ----------------------------------------

                    std::string keyName = GetKeyName(*key);

                    const float unbindWidth = 58.0f;
                    const float spacing = 6.0f;

                    ImVec2 keyTextSize =
                        ImGui::CalcTextSize(keyName.c_str());

                    float windowRight =
                        ImGui::GetWindowWidth() -
                        ImGui::GetStyle().WindowPadding.x;

                    float unbindX =
                        windowRight -
                        unbindWidth;

                    float keyX =
                        unbindX -
                        spacing -
                        keyTextSize.x;

                    ImGui::SameLine();

                    if (keyX > ImGui::GetCursorPosX())
                        ImGui::SetCursorPosX(keyX);

                    ImGui::TextDisabled(
                        "%s",
                        keyName.c_str()
                    );

                    // ----------------------------------------
                    // Unbind button
                    // ----------------------------------------

                    ImGui::SameLine();

                    ImGui::SetCursorPosX(unbindX);

                    bool alreadyUnbound = (*key == 0);

                    if (alreadyUnbound)
                        ImGui::BeginDisabled();

                    if (ImGui::Button(
                        "Unbind",
                        ImVec2(unbindWidth, 21.0f)
                    ))
                    {
                        *key = 0;
                    }

                    if (alreadyUnbound)
                        ImGui::EndDisabled();

                    ImGui::SetCursorPosY(
                        startY + 25.0f
                    );

                    // Submit an actual item so ImGui knows
                    // the window should extend to this position.
                    ImGui::Dummy(ImVec2(0.0f, 0.0f));

                    ImGui::PopID();
                };

            // ========================================================
            // AIMING
            // ========================================================

            HotkeyRow(
                "Aimbot",
                &Vars::Aimbot::aimbotKey
            );

            HotkeyRow(
                "Toggle Aimbot",
                &Vars::Aimbot::toggleKey
            );

            HotkeyRow(
                "Silent Aim",
                &Vars::Aimbot::silentAimbotKey
            );

            HotkeyRow(
                "Triggerbot",
                &Vars::Triggerbot::key
            );

            // ========================================================
            // PLAYER / MOVEMENT
            // ========================================================

            HotkeyRow(
                "Desync",
                &Vars::Local::desyncKey
            );

            HotkeyRow(
                "Flight",
                &Vars::Local::flightKey
            );

            HotkeyRow(
                "Freecam",
                &Vars::Local::freecamKey
            );
        }

        ImGui::End();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }

    void RenderLuaEditor()
    {
        if (!Vars::Utility::luaEditorOpen)
        {
            MonacoHost::SetVisible(false);
            return;
        }

        using namespace ExecutorUI;

        EnsureTab();

        // Compact like the reference instead of a giant fullscreen panel.
        ImGui::SetNextWindowSize(
            ImVec2(770.0f, 535.0f),
            ImGuiCond_FirstUseEver
        );

        ImGui::SetNextWindowPos(
            ImVec2(285.0f, 135.0f),
            ImGuiCond_FirstUseEver
        );

        // ------------------------------------------------------------
        // LOCAL STYLE
        // ------------------------------------------------------------

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowPadding,
            ImVec2(0, 0)
        );

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowRounding,
            6.0f
        );

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowBorderSize,
            1.0f
        );

        ImGui::PushStyleVar(
            ImGuiStyleVar_ItemSpacing,
            ImVec2(5.0f, 4.0f)
        );

        ImGui::PushStyleColor(
            ImGuiCol_WindowBg,
            WindowBg()
        );

        ImGui::PushStyleColor(
            ImGuiCol_Border,
            Border()
        );

        ImGui::PushStyleColor(
            ImGuiCol_MenuBarBg,
            TopBarBg()
        );

        ImGui::Begin(
            "Executor",
            &Vars::Utility::luaEditorOpen,
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_MenuBar
        );

        const ImVec2 pos =
            ImGui::GetWindowPos();

        const ImVec2 size =
            ImGui::GetWindowSize();

        Vars::Utility::luaX = pos.x;
        Vars::Utility::luaY = pos.y;
        Vars::Utility::luaW = size.x;
        Vars::Utility::luaH = size.y;

        const ImVec4 accent =
            Accent();

        // ============================================================
        // TOP BAR
        // Shrimplock  Luau  File  View  Run
        // ============================================================

        if (ImGui::BeginMenuBar())
        {
            ImGui::TextColored(
                accent,
                "Shrimplock"
            );

            ImGui::SameLine();

            ImGui::TextDisabled(
                "Luau"
            );

            ImGui::SameLine();

            if (ImGui::BeginMenu("File"))
            {
                if (
                    ImGui::MenuItem(
                        "New Script",
                        "Ctrl+N"
                    )
                    )
                {
                    NewScript();
                }

                if (
                    ImGui::MenuItem(
                        "Save",
                        "Ctrl+S"
                    )
                    )
                {
                    if (!SaveCurrent())
                    {
                        std::snprintf(
                            SaveAsName,
                            IM_ARRAYSIZE(
                                SaveAsName
                            ),
                            "%s",
                            Current().name.c_str()
                        );

                        SaveAsPopup = true;

                        ImGui::OpenPopup(
                            "Save Script As"
                        );
                    }
                }

                if (
                    ImGui::MenuItem(
                        "Save As...",
                        "Ctrl+Shift+S"
                    )
                    )
                {
                    std::snprintf(
                        SaveAsName,
                        IM_ARRAYSIZE(
                            SaveAsName
                        ),
                        "%s",
                        Current().name.c_str()
                    );

                    SaveAsPopup = true;

                    ImGui::OpenPopup(
                        "Save Script As"
                    );
                }

                ImGui::Separator();

                if (
                    ImGui::MenuItem(
                        "Open Scripts Folder"
                    )
                    )
                {
                    OpenScriptsFolder();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem(
                    "Lua Scripts",
                    nullptr,
                    &ShowScripts
                );

                ImGui::MenuItem(
                    "Current Script Terminal",
                    nullptr,
                    &ShowTerminal
                );

                ImGui::EndMenu();
            }

            // Run directly beside View, as requested.
            ImGui::PushStyleColor(
                ImGuiCol_Text,
                accent
            );

            if (
                ImGui::MenuItem(
                    "Run",
                    "F5"
                )
                )
            {
                RunCurrent();
            }

            ImGui::PopStyleColor();

            ImGui::EndMenuBar();
        }

        // ============================================================
        // SHORTCUTS
        // ============================================================

        ImGuiIO& io =
            ImGui::GetIO();

        if (
            ImGui::IsWindowFocused(
                ImGuiFocusedFlags_RootAndChildWindows
            )
            )
        {
            if (
                io.KeyCtrl &&
                !io.KeyShift &&
                ImGui::IsKeyPressed(
                    ImGuiKey_N,
                    false
                )
                )
            {
                NewScript();
            }

            if (
                io.KeyCtrl &&
                io.KeyShift &&
                ImGui::IsKeyPressed(
                    ImGuiKey_S,
                    false
                )
                )
            {
                std::snprintf(
                    SaveAsName,
                    IM_ARRAYSIZE(SaveAsName),
                    "%s",
                    Current().name.c_str()
                );

                SaveAsPopup = true;

                ImGui::OpenPopup(
                    "Save Script As"
                );
            }
            else if (
                io.KeyCtrl &&
                !io.KeyShift &&
                ImGui::IsKeyPressed(
                    ImGuiKey_S,
                    false
                )
                )
            {
                if (!SaveCurrent())
                {
                    std::snprintf(
                        SaveAsName,
                        IM_ARRAYSIZE(SaveAsName),
                        "%s",
                        Current().name.c_str()
                    );

                    SaveAsPopup = true;

                    ImGui::OpenPopup(
                        "Save Script As"
                    );
                }
            }

            if (
                ImGui::IsKeyPressed(
                    ImGuiKey_F5,
                    false
                )
                )
            {
                RunCurrent();
            }
        }

        // ============================================================
        // SAVE AS
        // ============================================================

        if (SaveAsPopup)
        {
            ImGui::OpenPopup(
                "Save Script As"
            );
        }

        ImGui::PushStyleColor(
            ImGuiCol_PopupBg,
            PanelBg()
        );

        ImGui::PushStyleColor(
            ImGuiCol_Border,
            Border()
        );

        if (
            ImGui::BeginPopupModal(
                "Save Script As",
                &SaveAsPopup,
                ImGuiWindowFlags_AlwaysAutoResize
            )
            )
        {
            ImGui::TextDisabled(
                "Save to scripts/"
            );

            ImGui::Spacing();

            ImGui::SetNextItemWidth(
                300.0f
            );

            ImGui::InputText(
                "##ScriptName",
                SaveAsName,
                IM_ARRAYSIZE(
                    SaveAsName
                )
            );

            ImGui::Spacing();

            ImGui::PushStyleColor(
                ImGuiCol_Button,
                AccentSoft(0.32f)
            );

            ImGui::PushStyleColor(
                ImGuiCol_ButtonHovered,
                AccentSoft(0.50f)
            );

            if (
                ImGui::Button(
                    "Save",
                    ImVec2(90, 27)
                )
                )
            {
                if (SaveAsName[0] != '\0')
                {
                    SaveTo(
                        Current(),
                        SaveAsName
                    );

                    SaveAsPopup = false;

                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::PopStyleColor(2);

            ImGui::SameLine();

            if (
                ImGui::Button(
                    "Cancel",
                    ImVec2(90, 27)
                )
                )
            {
                SaveAsPopup = false;

                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleColor(2);

        // ============================================================
        // MAIN BODY
        // ============================================================

        const float scriptsWidth =
            ShowScripts
            ? 150.0f
            : 0.0f;

        // ------------------------------------------------------------
        // LEFT: LUA SCRIPTS
        // ------------------------------------------------------------

        if (ShowScripts)
        {
            ImGui::PushStyleColor(
                ImGuiCol_ChildBg,
                PanelBg()
            );

            ImGui::PushStyleColor(
                ImGuiCol_Border,
                Border()
            );

            ImGui::PushStyleVar(
                ImGuiStyleVar_WindowPadding,
                ImVec2(9.0f, 8.0f)
            );

            ImGui::PushStyleVar(
                ImGuiStyleVar_ChildRounding,
                0.0f
            );

            ImGui::BeginChild(
                "##ScriptsSidebar",
                ImVec2(
                    scriptsWidth,
                    ImGui::GetContentRegionAvail().y
                ),
                true
            );

            ImGui::TextDisabled(
                "LUA SCRIPTS"
            );

            ImGui::SameLine();

            ImGui::SetCursorPosX(
                ImGui::GetWindowWidth()
                - 27.0f
            );

            ImGui::PushStyleColor(
                ImGuiCol_Button,
                AccentSoft(0.20f)
            );

            ImGui::PushStyleColor(
                ImGuiCol_ButtonHovered,
                AccentSoft(0.40f)
            );

            if (
                ImGui::SmallButton(
                    "+##NewScript"
                )
                )
            {
                NewScript();
            }

            ImGui::PopStyleColor(2);

            ImGui::Spacing();

            ImGui::Separator();

            ImGui::Spacing();

            ImGui::TextDisabled(
                "scripts"
            );

            ImGui::Spacing();

            const auto files =
                ScriptFiles();

            if (files.empty())
            {
                ImGui::TextDisabled(
                    "no scripts"
                );
            }
            else
            {
                ImGui::PushStyleColor(
                    ImGuiCol_Header,
                    AccentSoft(0.24f)
                );

                ImGui::PushStyleColor(
                    ImGuiCol_HeaderHovered,
                    AccentSoft(0.34f)
                );

                ImGui::PushStyleColor(
                    ImGuiCol_HeaderActive,
                    AccentSoft(0.42f)
                );

                for (
                    const auto& file :
                    files
                    )
                {
                    const bool selected =
                        !Current().path.empty() &&
                        Current().path == file;

                    const std::string name =
                        file.filename().string();

                    if (
                        ImGui::Selectable(
                            name.c_str(),
                            selected,
                            0,
                            ImVec2(
                                0.0f,
                                24.0f
                            )
                        )
                        )
                    {
                        LoadFile(file);
                    }
                }

                ImGui::PopStyleColor(3);
            }

            ImGui::EndChild();

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);

            ImGui::SameLine(
                0.0f,
                0.0f
            );
        }

        // ------------------------------------------------------------
        // RIGHT: EDITOR WORKSPACE
        // ------------------------------------------------------------

        ImGui::PushStyleColor(
            ImGuiCol_ChildBg,
            WindowBg()
        );

        ImGui::BeginChild(
            "##ExecutorEditorSide",
            ImVec2(0, 0),
            false
        );

        // ============================================================
        // TABS
        // ============================================================

        ImGui::PushStyleVar(
            ImGuiStyleVar_TabRounding,
            4.0f
        );

        ImGui::PushStyleColor(
            ImGuiCol_Tab,
            ImVec4(
                0.050f,
                0.052f,
                0.060f,
                1.0f
            )
        );

        ImGui::PushStyleColor(
            ImGuiCol_TabHovered,
            AccentSoft(0.22f)
        );

        ImGui::PushStyleColor(
            ImGuiCol_TabActive,
            AccentSoft(0.34f)
        );

        ImGui::PushStyleColor(
            ImGuiCol_TabUnfocused,
            ImVec4(
                0.043f,
                0.045f,
                0.052f,
                1.0f
            )
        );

        ImGui::PushStyleColor(
            ImGuiCol_TabUnfocusedActive,
            AccentSoft(0.20f)
        );

        if (
            ImGui::BeginTabBar(
                "##ExecutorTabs",
                ImGuiTabBarFlags_Reorderable |
                ImGuiTabBarFlags_AutoSelectNewTabs
            )
            )
        {
            int closeIndex = -1;

            for (
                int i = 0;
                i < static_cast<int>(
                    Tabs.size()
                    );
                ++i
                )
            {
                auto& tab =
                    Tabs[i];

                bool open = true;

                std::string title =
                    tab.name;

                if (tab.dirty)
                    title += " *";

                if (
                    ImGui::BeginTabItem(
                        title.c_str(),
                        &open
                    )
                    )
                {
                    CurrentTab = i;

                    ImGui::EndTabItem();
                }

                if (!open)
                    closeIndex = i;
            }

            ImGui::PushStyleColor(
                ImGuiCol_Text,
                accent
            );

            if (
                ImGui::TabItemButton(
                    "+",
                    ImGuiTabItemFlags_Trailing
                )
                )
            {
                NewScript();
            }

            ImGui::PopStyleColor();

            if (
                closeIndex >= 0 &&
                Tabs.size() > 1
                )
            {
                Tabs.erase(
                    Tabs.begin()
                    + closeIndex
                );

                if (
                    CurrentTab >=
                    static_cast<int>(
                        Tabs.size()
                        )
                    )
                {
                    CurrentTab =
                        static_cast<int>(
                            Tabs.size()
                            ) - 1;
                }
            }

            ImGui::EndTabBar();
        }

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar();

        // ============================================================
// MONACO + BOTTOM-BAR FIX FOR YOUR CURRENT MATCHA-STYLE EXECUTOR
//
// 1) Include MonacoEditor.h near your other headers:
//      #include "MonacoEditor.h"
//
// 2) IMPORTANT: WebView2 requires STA COM on the UI thread.
//    Change your current:
//      CoInitializeEx(nullptr, COINIT_MULTITHREADED);
//    to:
//      CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
//
// 3) Replace ONLY the EDITOR / OUTPUT / BOTTOM STATUS portion of
//    RenderLuaEditor() with this block.
//
// 4) At the very start of RenderLuaEditor(), BEFORE the early return:
//      if (!Vars::Utility::luaEditorOpen)
//      {
//          MonacoHost::SetVisible(false);
//          return;
//      }
//
// 5) Somewhere during shutdown (for example before destroying the
//    overlay window) call:
//      MonacoHost::Shutdown();
//
// ============================================================


// ============================================================
// AFTER YOUR TAB BAR:
// ============================================================

        auto& current =
            Current();

        MonacoHost::EnsureInitialized(
            windowHandle
        );

        // Keep Monaco synced when switching tabs.
        static int lastMonacoTab =
            -1;

        static std::string lastMonacoName;

        if (
            lastMonacoTab !=
            CurrentTab ||
            lastMonacoName !=
            current.name
            )
        {
            MonacoHost::SetText(
                Text(current)
            );

            lastMonacoTab =
                CurrentTab;

            lastMonacoName =
                current.name;
        }

        // Pull edits from Monaco back into ScriptTab::buffer.
        std::string monacoText;

        if (
            MonacoHost::ConsumeChangedText(
                monacoText
            )
            )
        {
            if (
                monacoText.size() >=
                current.buffer.size()
                )
            {
                monacoText.resize(
                    current.buffer.size() - 1
                );
            }

            current.buffer.fill('\0');

            std::copy(
                monacoText.begin(),
                monacoText.end(),
                current.buffer.begin()
            );

            current.buffer[
                monacoText.size()
            ] = '\0';

            current.dirty = true;
        }

        // Menu theme -> Monaco accent.
        MonacoHost::SetThemeAccent(
            Vars::Settings::themeColor[0],
            Vars::Settings::themeColor[1],
            Vars::Settings::themeColor[2]
        );


        // ============================================================
        // IMPORTANT LAYOUT FIX:
        //
        // Your old code was calculating the editor height manually, while
        // ImGui's item spacing/borders were still consuming pixels.
        // That is why the bottom status bar was getting clipped.
        //
        // This version reserves the bottom region FIRST, so OUTPUT and the
        // status bar always remain visible.
        // ============================================================

        const float outputHeaderHeight =
            26.0f;

        const float outputBodyHeight =
            ShowTerminal
            ? 112.0f
            : 0.0f;

        const float bottomStatusHeight =
            24.0f;

        const float reservedBottom =
            outputHeaderHeight +
            outputBodyHeight +
            bottomStatusHeight;


        // Remove vertical gaps between the editor/output/status stack.
        ImGui::PushStyleVar(
            ImGuiStyleVar_ItemSpacing,
            ImVec2(
                5.0f,
                0.0f
            )
        );


        // ============================================================
        // MONACO EDITOR SURFACE
        // ============================================================

        // Let ImGui calculate:
        // available height - everything that MUST stay at the bottom.
        const float editorHeight =
            (std::max)(
                110.0f,

                ImGui::GetContentRegionAvail().y
                - reservedBottom
                );

        const ImVec2 editorSize(
            ImGui::GetContentRegionAvail().x,
            editorHeight
        );

        // This invisible ImGui item reserves the exact Monaco area.
        // The real Monaco editor is a WebView2 child window placed over it.
        ImGui::InvisibleButton(
            "##MonacoEditorSurface",
            editorSize,
            ImGuiButtonFlags_MouseButtonLeft
        );

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            SetForegroundWindow(windowHandle);
            SetFocus(windowHandle);

            MonacoHost::Focus();
        }

        const ImVec2 monacoMin =
            ImGui::GetItemRectMin();

        const ImVec2 monacoMax =
            ImGui::GetItemRectMax();

        MonacoHost::SetBoundsFromScreenRect(
            windowHandle,
            monacoMin,
            monacoMax
        );

        // Hide Monaco while an ImGui popup is open so the child HWND doesn't
        // cover Save-As/File menu popups.
        const bool anyPopupOpen =
            ImGui::IsPopupOpen(
                nullptr,
                ImGuiPopupFlags_AnyPopupId
            );

        MonacoHost::SetVisible(
            Vars::Utility::luaEditorOpen &&
            !anyPopupOpen
        );


        // ============================================================
        // OUTPUT HEADER
        // ============================================================

        auto currentLogs =
            CurrentLogs();

        ImGui::PushStyleColor(
            ImGuiCol_ChildBg,
            TopBarBg()
        );

        ImGui::BeginChild(
            "##OutputHeader",
            ImVec2(
                0.0f,
                outputHeaderHeight
            ),
            false,
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse
        );

        ImGui::SetCursorPos(
            ImVec2(
                10.0f,
                5.0f
            )
        );

        ImGui::TextUnformatted(
            "OUTPUT"
        );

        ImGui::SameLine();

        ImGui::TextDisabled(
            "%d",
            static_cast<int>(
                currentLogs.size()
                )
        );

        ImGui::SameLine();

        ImGui::SetCursorPosX(
            ImGui::GetWindowWidth()
            - 108.0f
        );

        if (
            ImGui::SmallButton(
                "Copy"
            )
            )
        {
            std::string output;

            for (
                const auto& log :
                currentLogs
                )
            {
                output += "[";
                output +=
                    TimeString(
                        log.timestamp
                    );

                output += "] ";

                output +=
                    LogLabel(
                        log.type
                    );

                output += "  ";
                output += log.text;
                output += "\n";
            }

            ImGui::SetClipboardText(
                output.c_str()
            );
        }

        ImGui::SameLine();

        if (
            ImGui::SmallButton(
                "Clear"
            )
            )
        {
            ClearCurrentLogs();
        }

        ImGui::SameLine();

        if (
            ImGui::SmallButton(
                ShowTerminal
                ? "^"
                : "v"
            )
            )
        {
            ShowTerminal =
                !ShowTerminal;
        }

        ImGui::EndChild();

        ImGui::PopStyleColor();


        // ============================================================
        // OUTPUT BODY
        // ============================================================

        if (ShowTerminal)
        {
            ImGui::PushStyleColor(
                ImGuiCol_ChildBg,
                EditorBg()
            );

            ImGui::PushStyleColor(
                ImGuiCol_Border,
                Border()
            );

            ImGui::BeginChild(
                "##OutputBody",
                ImVec2(
                    0.0f,
                    outputBodyHeight
                ),
                true,
                ImGuiWindowFlags_AlwaysVerticalScrollbar
            );

            if (
                currentLogs.empty()
                )
            {
                const char* text =
                    "No output yet";

                const ImVec2 textSize =
                    ImGui::CalcTextSize(
                        text
                    );

                ImGui::SetCursorPos(
                    ImVec2(
                        (
                            ImGui::GetContentRegionAvail().x
                            - textSize.x
                            ) * 0.5f,

                        37.0f
                    )
                );

                ImGui::TextDisabled(
                    "%s",
                    text
                );
            }
            else
            {
                for (
                    const auto& log :
                    currentLogs
                    )
                {
                    const std::string time =
                        TimeString(
                            log.timestamp
                        );

                    ImGui::TextDisabled(
                        "[%s]",
                        time.c_str()
                    );

                    ImGui::SameLine();

                    ImGui::TextColored(
                        LogColor(
                            log.type
                        ),
                        "%-5s",
                        LogLabel(
                            log.type
                        )
                    );

                    ImGui::SameLine();

                    ImGui::TextWrapped(
                        "%s",
                        log.text.c_str()
                    );
                }

                if (
                    AutoScroll &&
                    ScrollToBottom
                    )
                {
                    ImGui::SetScrollHereY(
                        1.0f
                    );

                    ScrollToBottom =
                        false;
                }
            }

            ImGui::EndChild();

            ImGui::PopStyleColor(2);
        }


        // ============================================================
        // BOTTOM STATUS - NOW GUARANTEED TO STAY VISIBLE
        // ============================================================

        int lines =
            1;

        for (
            const char* p =
            current.buffer.data();
            *p;
            ++p
            )
        {
            if (*p == '\n')
                ++lines;
        }

        ImGui::PushStyleColor(
            ImGuiCol_ChildBg,
            TopBarBg()
        );

        ImGui::BeginChild(
            "##ExecutorStatus",
            ImVec2(
                0.0f,
                bottomStatusHeight
            ),
            false,
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse
        );

        ImGui::SetCursorPos(
            ImVec2(
                9.0f,
                4.0f
            )
        );

        ImGui::TextDisabled(
            "scripts"
        );

        ImGui::SameLine();

        ImGui::TextColored(
            Accent(),
            ">"
        );

        ImGui::SameLine();

        ImGui::TextDisabled(
            "%s%s",
            current.name.c_str(),
            current.dirty
            ? " *"
            : ""
        );

        char rightStatus[160]{};

        std::snprintf(
            rightStatus,
            sizeof(rightStatus),

            "Ln %d, Col %d   |   %d lines   |   Lua",

            MonacoHost::CursorLine(),
            MonacoHost::CursorColumn(),
            lines
        );

        const float rightWidth =
            ImGui::CalcTextSize(
                rightStatus
            ).x;

        ImGui::SameLine();

        ImGui::SetCursorPosX(
            (std::max)(
                ImGui::GetCursorPosX()
                + 20.0f,

                ImGui::GetWindowWidth()
                - rightWidth
                - 9.0f
                )
        );

        ImGui::TextDisabled(
            "%s",
            rightStatus
        );

        ImGui::EndChild();

        ImGui::PopStyleColor();


        // Restore your normal ImGui item spacing.
        ImGui::PopStyleVar();


        // ============================================================
        // DELETE your old:
        //   InputTextMultiline(...)
        //   old OUTPUT header/body
        //   old bottom status bar
        //
        // Keep the tab bar, sidebar, File/View/Run, Save As, etc.
        // ============================================================


        ImGui::EndChild();

        ImGui::PopStyleColor();

        ImGui::End();

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(4);
    }


    void HotkeyButton(const char* label, int* k) {
        ImGui::Text("%s", label);
        ImGui::SameLine(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 110.0f);
        ImGui::PushID(label);
        
        static int* bindingKey = nullptr;
        bool isBinding = bindingKey == k;
        static DWORD waitTime = 0;
        
        std::string buttonText = isBinding ? "Press key..." : GetKeyName(*k);
        if (ImGui::Button(buttonText.c_str(), ImVec2(100, 25))) {
            bindingKey = k;
            waitTime = GetTickCount64() + 200;
        }

        if (isBinding && GetTickCount64() > waitTime) {
            for (int i = 1; i < 256; i++) {
                if (GetAsyncKeyState(i) & 0x8000) {
                    if (i == VK_ESCAPE) *k = 0;
                    else *k = i;
                    bindingKey = nullptr;
                    break;
                }
            }
        }
        ImGui::PopID();
    }

    bool Initialize() {

        CoInitializeEx(
            nullptr,
            COINIT_APARTMENTTHREADED
        );

        windowClass.cbSize = sizeof(WNDCLASSEXW);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = OverlayWndProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);

        windowClass.lpszClassName = L"Shrimplock";

        if (!RegisterClassExW(&windowClass))
            return false;

        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);

        windowHandle = CreateWindowExW(
            WS_EX_TOPMOST |
            WS_EX_LAYERED |
            WS_EX_TOOLWINDOW |
            WS_EX_NOACTIVATE |
            WS_EX_TRANSPARENT,

            windowClass.lpszClassName,
            L"Shrimplock",

            WS_POPUP,
            0, 0,
            screenW,
            screenH,

            nullptr,
            nullptr,
            windowClass.hInstance,
            nullptr
        );

        if (!windowHandle)
            return false;

        SetLayeredWindowAttributes(windowHandle, RGB(0, 0, 0), 255, LWA_ALPHA);

        MARGINS margins = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(windowHandle, &margins);

        ShowWindow(windowHandle, SW_SHOWNOACTIVATE);
        UpdateWindow(windowHandle);

        SetupD3D11(windowHandle);

        // Download + load GitHub logo
        LoadLoadingLogo();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;

        // Smoother UI font
        ImFontConfig fontConfig{};
        fontConfig.OversampleH = 3;
        fontConfig.OversampleV = 2;
        fontConfig.PixelSnapH = false;

        io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\segoeui.ttf",
            15.0f,
            &fontConfig
        );

        // Fall back to ImGui's default font if Segoe UI couldn't load
        if (io.Fonts->Fonts.empty())
            io.Fonts->AddFontDefault();

        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();

        style.Alpha = 1.0f;
        style.WindowRounding = 6.0f;
        style.FrameRounding = 5.0f;
        style.PopupRounding = 6.0f;

        // Scrollbar
        style.ScrollbarSize = 5.0f;
        style.ScrollbarRounding = 2.5f;

        style.GrabRounding = 5.0f;
        style.TabRounding = 5.0f;
        style.ChildRounding = 6.0f;

        style.FrameBorderSize = 0.0f;
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;

        style.ItemSpacing = ImVec2(6, 4);
        style.WindowPadding = ImVec2(7, 7);
        style.FramePadding = ImVec2(5, 2);

        ImVec4* colors = style.Colors;

        ImVec4 accentColor = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        ImVec4 accentHover = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
        ImVec4 accentActive = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

        colors[ImGuiCol_WindowBg] = ImVec4(0.035f, 0.035f, 0.035f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.047f, 0.047f, 0.047f, 1.0f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.050f, 0.050f, 0.050f, 1.00f);

        colors[ImGuiCol_Border] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

        colors[ImGuiCol_FrameBg] = ImVec4(0.095f, 0.095f, 0.10f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.12f, 0.13f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.14f, 0.14f, 0.15f, 1.00f);

        colors[ImGuiCol_TitleBg] = ImVec4(0.035f, 0.035f, 0.035f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.035f, 0.035f, 0.035f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.035f, 0.035f, 0.035f, 1.00f);

        colors[ImGuiCol_Button] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = accentHover;
        colors[ImGuiCol_ButtonActive] = accentActive;

        colors[ImGuiCol_Header] = accentColor;
        colors[ImGuiCol_HeaderHovered] = accentHover;
        colors[ImGuiCol_HeaderActive] = accentActive;

        colors[ImGuiCol_CheckMark] = accentColor;
        colors[ImGuiCol_SliderGrab] = accentColor;
        colors[ImGuiCol_SliderGrabActive] = accentActive;

        colors[ImGuiCol_Text] = ImVec4(0.82f, 0.82f, 0.84f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.45f, 0.47f, 1.00f);

        colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
        colors[ImGuiCol_SeparatorHovered] = accentColor;
        colors[ImGuiCol_SeparatorActive] = accentActive;

        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.035f, 0.035f, 0.035f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);

        ImGui_ImplWin32_Init(windowHandle);
        ImGui_ImplDX11_Init(d3dDevice, d3dContext);

        return true;
    }

    bool IsRobloxFocused()
    {
        HWND foreground = GetForegroundWindow();
        if (!foreground)
            return false;

        // Keep overlay alive while clicking the menu
        if (foreground == windowHandle)
            return true;

        DWORD pid = 0;
        GetWindowThreadProcessId(foreground, &pid);

        if (!pid)
            return false;

        HANDLE process = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION,
            FALSE,
            pid
        );

        if (!process)
            return false;

        char path[MAX_PATH]{};
        DWORD size = MAX_PATH;

        bool isRoblox = false;

        if (QueryFullProcessImageNameA(process, 0, path, &size))
        {
            std::string exePath(path);

            size_t slash = exePath.find_last_of("\\/");
            std::string exeName =
                (slash != std::string::npos)
                ? exePath.substr(slash + 1)
                : exePath;

            isRoblox =
                _stricmp(exeName.c_str(), "RobloxPlayerBeta.exe") == 0;
        }

        CloseHandle(process);
        return isRoblox;
    }

    void BeginFrame() {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        bool robloxFocused = IsRobloxFocused();

        // Hide overlay whenever Roblox isn't the active window
        if (!robloxFocused)
        {
            ShowWindow(windowHandle, SW_HIDE);
        }
        else
        {
            ShowWindow(windowHandle, SW_SHOWNA);

            POINT cursor{};
            GetCursorPos(&cursor);

            bool watermarkHovered = false;

            if (Vars::Settings::showWatermark &&
                Vars::Settings::watermarkX >= 0.0f)
            {
                watermarkHovered =
                    cursor.x >= Vars::Settings::watermarkX &&
                    cursor.x <= Vars::Settings::watermarkX + Vars::Settings::watermarkWidth &&
                    cursor.y >= Vars::Settings::watermarkY &&
                    cursor.y <= Vars::Settings::watermarkY + Vars::Settings::watermarkHeight;
            }

            bool keybindHovered = false;
            bool luaHovered = false;
            bool musicHovered = false;
            bool explorerHovered = false;

            bool playerActionsHovered = false;
            bool flingTargetsHovered = false;

            // ==============================
            // HOTKEY WINDOW
            // ==============================
            if (Vars::Utility::keybindsOpen &&
                Vars::Utility::keybindX >= 0.0f)
            {
                keybindHovered =
                    cursor.x >= Vars::Utility::keybindX &&
                    cursor.x <= Vars::Utility::keybindX + Vars::Utility::keybindW &&
                    cursor.y >= Vars::Utility::keybindY &&
                    cursor.y <= Vars::Utility::keybindY + Vars::Utility::keybindH;
            }

            // ==============================
            // LUA EDITOR
            // ==============================
            if (Vars::Utility::luaEditorOpen &&
                Vars::Utility::luaX >= 0.0f)
            {
                luaHovered =
                    cursor.x >= Vars::Utility::luaX &&
                    cursor.x <= Vars::Utility::luaX + Vars::Utility::luaW &&
                    cursor.y >= Vars::Utility::luaY &&
                    cursor.y <= Vars::Utility::luaY + Vars::Utility::luaH;
            }

            // ==============================
// MUSIC
// ==============================
            if (Vars::Music::menuOpen &&
                Vars::Music::windowX >= 0.0f)
            {
                musicHovered =
                    cursor.x >= Vars::Music::windowX &&
                    cursor.x <= Vars::Music::windowX + Vars::Music::windowW &&
                    cursor.y >= Vars::Music::windowY &&
                    cursor.y <= Vars::Music::windowY + Vars::Music::windowH;
            }

            // ==============================
            // EXPLORER
            // ==============================
            if (Vars::Explorer::enabled &&
                Vars::Explorer::windowX >= 0.0f)
            {
                explorerHovered =
                    cursor.x >= Vars::Explorer::windowX &&
                    cursor.x <= Vars::Explorer::windowX + Vars::Explorer::windowW &&
                    cursor.y >= Vars::Explorer::windowY &&
                    cursor.y <= Vars::Explorer::windowY + Vars::Explorer::windowH;
            }

            // ==============================
// PLAYER ACTIONS
// ==============================

            if (
                Vars::Misc::playerActionsOpen &&
                Vars::Misc::actionsX >= 0.0f
                )
            {
                playerActionsHovered =
                    cursor.x >=
                    Vars::Misc::actionsX &&

                    cursor.x <=
                    Vars::Misc::actionsX +
                    Vars::Misc::actionsW &&

                    cursor.y >=
                    Vars::Misc::actionsY &&

                    cursor.y <=
                    Vars::Misc::actionsY +
                    Vars::Misc::actionsH;
            }

            // ==============================
            // FLING TARGETS
            // ==============================

            if (
                Vars::Misc::flingTargetsWindowOpen &&
                Vars::Misc::flingWindowX >= 0.0f
                )
            {
                flingTargetsHovered =
                    cursor.x >=
                    Vars::Misc::flingWindowX &&

                    cursor.x <=
                    Vars::Misc::flingWindowX +
                    Vars::Misc::flingWindowW &&

                    cursor.y >=
                    Vars::Misc::flingWindowY &&

                    cursor.y <=
                    Vars::Misc::flingWindowY +
                    Vars::Misc::flingWindowH;
            }

            bool enemyListHovered = false;

            if (
                Vars::Misc::enemyListWindowOpen &&
                Vars::Misc::enemyWindowX >= 0.0f
                )
            {
                enemyListHovered =
                    cursor.x >= Vars::Misc::enemyWindowX &&
                    cursor.x <= Vars::Misc::enemyWindowX + Vars::Misc::enemyWindowW &&
                    cursor.y >= Vars::Misc::enemyWindowY &&
                    cursor.y <= Vars::Misc::enemyWindowY + Vars::Misc::enemyWindowH;
            }

            // ==============================
// OVERLAY INPUT / FOCUS
// ==============================

            bool explorerInput =
                Explorer::IsMouseOverExplorerWindows();

            LONG_PTR exStyle =
                GetWindowLongPtr(
                    windowHandle,
                    GWL_EXSTYLE
                );

            exStyle |= WS_EX_TOPMOST;
            exStyle |= WS_EX_LAYERED;
            exStyle |= WS_EX_TOOLWINDOW;

            if (Vars::menuOpen)
            {
                exStyle &= ~WS_EX_TRANSPARENT;
                exStyle &= ~WS_EX_NOACTIVATE;
            }
            else if (explorerInput)
            {
                exStyle &= ~WS_EX_TRANSPARENT;
                exStyle |= WS_EX_NOACTIVATE;
            }
            else
            {
                exStyle |= WS_EX_TRANSPARENT;
                exStyle |= WS_EX_NOACTIVATE;
            }

            SetWindowLongPtr(
                windowHandle,
                GWL_EXSTYLE,
                exStyle
            );
        }

        SetWindowDisplayAffinity(
            windowHandle,
            Vars::Misc::streamProof
            ? WDA_EXCLUDEFROMCAPTURE
            : WDA_NONE
        );

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void DrawLocalAvatarPreview(
        ImDrawList* dl,
        const ImVec2& areaMin,
        const ImVec2& areaMax,
        float alpha,
        float yaw)
    {
        auto character =
            Globals::localPlayer.GetModelRef();

        if (character.Addr == 0)
            return;

        auto root =
            character.FindCharacterPart("HumanoidRootPart");

        if (root.Addr == 0)
            root = character.FindCharacterPart("Root");

        if (root.Addr == 0)
            return;

        RBX::Vec3 rootPos = root.GetPos();

        struct PreviewPart
        {
            RBX::RbxInstance instance;

            float width;
            float height;
            float depth;

            float sortDepth;
        };

        std::vector<PreviewPart> parts;

        auto AddPart =
            [&](const char* name,
                float width,
                float height,
                float depth)
            {
                auto part =
                    character.FindCharacterPart(name);

                if (part.Addr == 0)
                    return;

                parts.push_back(
                    {
                        part,
                        width,
                        height,
                        depth,
                        0.0f
                    }
                );
            };

        // ============================================================
        // DETECT R6 / R15
        // ============================================================

        auto torso =
            character.FindCharacterPart("Torso");

        bool isR6 =
            torso.Addr != 0;

        if (isR6)
        {
            AddPart(
                "Head",
                2.0f,
                1.0f,
                1.0f
            );

            AddPart(
                "Torso",
                2.0f,
                2.0f,
                1.0f
            );

            AddPart(
                "Left Arm",
                1.0f,
                2.0f,
                1.0f
            );

            AddPart(
                "Right Arm",
                1.0f,
                2.0f,
                1.0f
            );

            AddPart(
                "Left Leg",
                1.0f,
                2.0f,
                1.0f
            );

            AddPart(
                "Right Leg",
                1.0f,
                2.0f,
                1.0f
            );
        }
        else
        {
            AddPart(
                "Head",
                1.6f,
                1.0f,
                1.0f
            );

            AddPart(
                "UpperTorso",
                1.6f,
                1.6f,
                0.8f
            );

            AddPart(
                "LowerTorso",
                1.4f,
                0.8f,
                0.8f
            );

            AddPart(
                "LeftUpperArm",
                0.8f,
                1.2f,
                0.8f
            );

            AddPart(
                "LeftLowerArm",
                0.7f,
                1.2f,
                0.7f
            );

            AddPart(
                "LeftHand",
                0.7f,
                0.7f,
                0.7f
            );

            AddPart(
                "RightUpperArm",
                0.8f,
                1.2f,
                0.8f
            );

            AddPart(
                "RightLowerArm",
                0.7f,
                1.2f,
                0.7f
            );

            AddPart(
                "RightHand",
                0.7f,
                0.7f,
                0.7f
            );

            AddPart(
                "LeftUpperLeg",
                0.8f,
                1.3f,
                0.8f
            );

            AddPart(
                "LeftLowerLeg",
                0.7f,
                1.3f,
                0.7f
            );

            AddPart(
                "LeftFoot",
                0.8f,
                0.5f,
                1.1f
            );

            AddPart(
                "RightUpperLeg",
                0.8f,
                1.3f,
                0.8f
            );

            AddPart(
                "RightLowerLeg",
                0.7f,
                1.3f,
                0.7f
            );

            AddPart(
                "RightFoot",
                0.8f,
                0.5f,
                1.1f
            );
        }

        if (parts.empty())
            return;

        // ============================================================
        // PREVIEW CAMERA
        // ============================================================

        float c =
            std::cos(yaw);

        float s =
            std::sin(yaw);

        const float areaWidth =
            areaMax.x - areaMin.x;

        const float areaHeight =
            areaMax.y - areaMin.y;

        const ImVec2 center(
            areaMin.x +
            areaWidth * 0.5f,

            areaMin.y +
            areaHeight * 0.53f
        );

        // Bigger number = larger avatar.
        const float scale =
            28.0f;

        auto RotatePoint =
            [&](const RBX::Vec3& world) -> RBX::Vec3
            {
                RBX::Vec3 rel =
                {
                    world.X - rootPos.X,
                    world.Y - rootPos.Y,
                    world.Z - rootPos.Z
                };

                return
                {
                    rel.X * c -
                    rel.Z * s,

                    rel.Y,

                    rel.X * s +
                    rel.Z * c
                };
            };

        auto ProjectPreview =
            [&](const RBX::Vec3& point) -> ImVec2
            {
                return ImVec2(
                    center.x +
                    point.X * scale,

                    center.y -
                    point.Y * scale
                );
            };

        // ============================================================
        // CALCULATE DEPTH
        // ============================================================

        for (auto& p : parts)
        {
            RBX::Vec3 centerPos =
                RotatePoint(
                    p.instance.GetPos()
                );

            p.sortDepth =
                centerPos.Z;
        }

        // Draw far parts first.
        std::sort(
            parts.begin(),
            parts.end(),
            [](const PreviewPart& a,
                const PreviewPart& b)
            {
                return
                    a.sortDepth <
                    b.sortDepth;
            }
        );

        // Keep avatar inside preview window.
        dl->PushClipRect(
            areaMin,
            areaMax,
            true
        );

        // ============================================================
        // DRAW BODY PARTS
        // ============================================================

        for (auto& p : parts)
        {
            RBX::Vec3 partCenter =
                p.instance.GetPos();

            RBX::Rotation rotation =
                p.instance.GetRotation();

            RBX::Vec3 right =
                rotation.GetRightVector();

            RBX::Vec3 up =
                rotation.GetUpVector();

            RBX::Vec3 forward =
                rotation.GetLookVector();

            RBX::Vec3 corners[8];

            Visuals::BuildOBBCorners(
                partCenter,
                right,
                up,
                forward,
                p.width * 0.5f,
                p.height * 0.5f,
                p.depth * 0.5f,
                corners
            );

            std::vector<ImVec2> projected;
            projected.reserve(8);

            for (int i = 0; i < 8; ++i)
            {
                RBX::Vec3 rotated =
                    RotatePoint(
                        corners[i]
                    );

                projected.push_back(
                    ProjectPreview(rotated)
                );
            }

            std::vector<ImVec2> hull =
                Visuals::MakeConvexPolygon(
                    projected
                );

            if (hull.size() < 3)
                continue;

            // Slight depth shading.
            float depthShade =
                0.78f -
                (p.sortDepth * 0.025f);

            depthShade =
                std::clamp(
                    depthShade,
                    0.48f,
                    0.88f
                );

            int shade =
                static_cast<int>(
                    255.0f * depthShade
                    );

            int bodyAlpha =
                static_cast<int>(
                    210.0f * alpha
                    );

            ImU32 fillColor =
                IM_COL32(
                    shade,
                    shade,
                    shade,
                    bodyAlpha
                );

            ImU32 outlineColor =
                IM_COL32(
                    15,
                    15,
                    15,
                    static_cast<int>(
                        245.0f * alpha
                        )
                );

            dl->AddConvexPolyFilled(
                hull.data(),
                static_cast<int>(hull.size()),
                fillColor
            );

            dl->AddPolyline(
                hull.data(),
                static_cast<int>(hull.size()),
                outlineColor,
                ImDrawFlags_Closed,
                1.5f
            );
        }

        dl->PopClipRect();
    }

    void RenderESPPreview(float alpha, const ImVec2& menuPos, const ImVec2& menuSize)
    {
        if (Vars::selectedTab != 1 || alpha <= 0.01f || !Vars::ESP::enabled)
            return;

        const float gap = 12.0f;
        const ImVec2 previewSize(245.0f, 430.0f);

        // Default position beside menu, but user can move it afterward.
        ImGui::SetNextWindowPos(
            ImVec2(
                menuPos.x + menuSize.x + gap,
                menuPos.y
            ),
            ImGuiCond_Always
        );

        ImGui::SetNextWindowSize(previewSize, ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGui::Begin(
            "##ESPPreview",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBackground
        );

        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();

        ImDrawList* dl = ImGui::GetWindowDrawList();

        int a = static_cast<int>(255.0f * alpha);

        // ============================================================
        // WINDOW
        // ============================================================

        dl->AddRectFilled(
            pos,
            pos + size,
            IM_COL32(12, 12, 12, a),
            6.0f
        );

        dl->AddText(
            pos + ImVec2(12, 10),
            IM_COL32(220, 220, 220, a),
            "ESP Preview"
        );

        dl->AddLine(
            pos + ImVec2(10, 34),
            pos + ImVec2(size.x - 10, 34),
            IM_COL32(45, 45, 45, a)
        );

        // ============================================================
        // PLAYER BOUNDS
        // ============================================================

        float centerX =
            pos.x +
            size.x * 0.5f;

        float minX =
            centerX - 42.0f;

        float maxX =
            centerX + 42.0f;

        float minY =
            pos.y + 105.0f;

        float maxY =
            pos.y + 310.0f;

        float boxWidth =
            maxX - minX;

        float boxHeight =
            maxY - minY;

        // ============================================================
        // ROTATABLE LOCAL AVATAR
        // ============================================================

        static float avatarYaw =
            0.0f;

        ImVec2 avatarAreaMin(
            pos.x + 20.0f,
            pos.y + 48.0f
        );

        ImVec2 avatarAreaMax(
            pos.x + size.x - 20.0f,
            pos.y + 335.0f
        );

        // Invisible interaction surface.
        ImVec2 oldCursor =
            ImGui::GetCursorScreenPos();

        ImGui::SetCursorScreenPos(
            avatarAreaMin
        );

        ImGui::InvisibleButton(
            "##ESPAvatarRotation",
            ImVec2(
                avatarAreaMax.x -
                avatarAreaMin.x,

                avatarAreaMax.y -
                avatarAreaMin.y
            ),
            ImGuiButtonFlags_MouseButtonLeft
        );

        if (ImGui::IsItemActive() &&
            ImGui::IsMouseDragging(
                ImGuiMouseButton_Left,
                0.0f
            ))
        {
            avatarYaw +=
                ImGui::GetIO().MouseDelta.x *
                0.012f;
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(
                ImGuiMouseCursor_ResizeEW
            );
        }

        ImGui::SetCursorScreenPos(
            oldCursor
        );

        // Render actual local character underneath the ESP.
        DrawLocalAvatarPreview(
            dl,
            avatarAreaMin,
            avatarAreaMax,
            alpha,
            avatarYaw
        );

        // ============================================================
// ESP COLORS
// ============================================================

        auto& bc = Vars::ESP::boxColor;

        ImU32 boxCol = IM_COL32(
            static_cast<int>(bc[0] * 255),
            static_cast<int>(bc[1] * 255),
            static_cast<int>(bc[2] * 255),
            static_cast<int>(bc[3] * 255 * alpha)
        );

        auto& bgc = Vars::ESP::boxGradientColor;

        ImU32 boxGradCol = IM_COL32(
            static_cast<int>(bgc[0] * 255),
            static_cast<int>(bgc[1] * 255),
            static_cast<int>(bgc[2] * 255),
            static_cast<int>(bgc[3] * 255 * alpha)
        );

        // ============================================================
        // RAINBOW ESP PREVIEW
        // ============================================================

        if (Vars::ESP::rainbowBox)
        {
            float hue = fmodf(
                static_cast<float>(ImGui::GetTime()) *
                Vars::ESP::rainbowSpeed,
                1.0f
            );

            ImVec4 rainbow;

            ImGui::ColorConvertHSVtoRGB(
                hue,
                1.0f,
                1.0f,
                rainbow.x,
                rainbow.y,
                rainbow.z
            );

            rainbow.w = bc[3] * alpha;

            boxCol =
                ImGui::ColorConvertFloat4ToU32(rainbow);

            // Second rainbow color for Gradient Mode
            float gradientHue =
                fmodf(hue + 0.18f, 1.0f);

            ImVec4 rainbowGradient;

            ImGui::ColorConvertHSVtoRGB(
                gradientHue,
                1.0f,
                1.0f,
                rainbowGradient.x,
                rainbowGradient.y,
                rainbowGradient.z
            );

            rainbowGradient.w =
                bgc[3] * alpha;

            boxGradCol =
                ImGui::ColorConvertFloat4ToU32(
                    rainbowGradient
                );
        }

        float thick = Vars::ESP::boxThickness;

        // ============================================================
        // FILLED BOX
        // ============================================================

        if (Vars::ESP::filledBox)
        {
            auto& fc = Vars::ESP::filledColor;

            ImU32 fillCol = IM_COL32(
                static_cast<int>(fc[0] * 255),
                static_cast<int>(fc[1] * 255),
                static_cast<int>(fc[2] * 255),
                static_cast<int>(fc[3] * 255 * alpha)
            );

            dl->AddRectFilled(
                ImVec2(minX, minY),
                ImVec2(maxX, maxY),
                fillCol
            );
        }

        // ============================================================
        // NORMAL BOX
        // ============================================================

        if (Vars::ESP::boxMode == 1)
        {
            dl->AddRect(
                ImVec2(minX - 1, minY - 1),
                ImVec2(maxX + 1, maxY + 1),
                IM_COL32(0, 0, 0, static_cast<int>(220 * alpha)),
                0.0f,
                0,
                thick + 2.0f
            );

            if (Vars::ESP::boxGradient)
            {
                dl->AddRectFilledMultiColor(
                    ImVec2(minX, minY),
                    ImVec2(minX + thick, maxY),
                    boxCol,
                    boxCol,
                    boxGradCol,
                    boxGradCol
                );

                dl->AddRectFilledMultiColor(
                    ImVec2(maxX - thick, minY),
                    ImVec2(maxX, maxY),
                    boxCol,
                    boxCol,
                    boxGradCol,
                    boxGradCol
                );

                dl->AddRectFilled(
                    ImVec2(minX, minY),
                    ImVec2(maxX, minY + thick),
                    boxCol
                );

                dl->AddRectFilled(
                    ImVec2(minX, maxY - thick),
                    ImVec2(maxX, maxY),
                    boxGradCol
                );
            }
            else
            {
                dl->AddRect(
                    ImVec2(minX, minY),
                    ImVec2(maxX, maxY),
                    boxCol,
                    0.0f,
                    0,
                    thick
                );
            }
        }

        // ============================================================
        // CORNER BOX
        // ============================================================

        else if (Vars::ESP::boxMode == 2)
        {
            float lineW = boxWidth / 4.0f;
            float lineH = boxHeight / 4.0f;

            dl->AddLine(
                ImVec2(minX, minY),
                ImVec2(minX + lineW, minY),
                boxCol,
                thick
            );

            dl->AddLine(
                ImVec2(minX, minY),
                ImVec2(minX, minY + lineH),
                boxCol,
                thick
            );

            dl->AddLine(
                ImVec2(maxX, minY),
                ImVec2(maxX - lineW, minY),
                boxCol,
                thick
            );

            dl->AddLine(
                ImVec2(maxX, minY),
                ImVec2(maxX, minY + lineH),
                boxCol,
                thick
            );

            dl->AddLine(
                ImVec2(minX, maxY),
                ImVec2(minX + lineW, maxY),
                boxGradCol,
                thick
            );

            dl->AddLine(
                ImVec2(minX, maxY),
                ImVec2(minX, maxY - lineH),
                boxGradCol,
                thick
            );

            dl->AddLine(
                ImVec2(maxX, maxY),
                ImVec2(maxX - lineW, maxY),
                boxGradCol,
                thick
            );

            dl->AddLine(
                ImVec2(maxX, maxY),
                ImVec2(maxX, maxY - lineH),
                boxGradCol,
                thick
            );
        }

        // ============================================================
        // SKELETON PREVIEW
        // ============================================================

        if (Vars::ESP::skeleton)
        {
            auto& sc = Vars::ESP::skeletonColor;

            ImU32 skeletonCol = IM_COL32(
                static_cast<int>(sc[0] * 255),
                static_cast<int>(sc[1] * 255),
                static_cast<int>(sc[2] * 255),
                static_cast<int>(sc[3] * 255 * alpha)
            );

            float skeletonThickness =
                Vars::ESP::skeletonThickness;

            ImU32 skeletonShadow =
                IM_COL32(0, 0, 0, static_cast<int>(200 * alpha));

            // R6 skeleton joints
            ImVec2 headJoint(centerX, minY + 27.0f);
            ImVec2 neckJoint(centerX, minY + 49.0f);

            ImVec2 torsoCenter(centerX, minY + 81.0f);
            ImVec2 hipCenter(centerX, minY + 112.0f);

            ImVec2 leftShoulder(centerX - 22.0f, minY + 55.0f);
            ImVec2 rightShoulder(centerX + 22.0f, minY + 55.0f);

            ImVec2 leftHand(centerX - 31.0f, minY + 110.0f);
            ImVec2 rightHand(centerX + 31.0f, minY + 110.0f);

            ImVec2 leftFoot(centerX - 11.0f, maxY - 8.0f);
            ImVec2 rightFoot(centerX + 11.0f, maxY - 8.0f);

            auto DrawPreviewBone =
                [&](ImVec2 p1, ImVec2 p2)
                {
                    dl->AddLine(
                        p1,
                        p2,
                        skeletonShadow,
                        skeletonThickness + 2.0f
                    );

                    dl->AddLine(
                        p1,
                        p2,
                        skeletonCol,
                        skeletonThickness
                    );
                };

            // Head -> torso
            DrawPreviewBone(headJoint, neckJoint);
            DrawPreviewBone(neckJoint, torsoCenter);
            DrawPreviewBone(torsoCenter, hipCenter);

            // Arms
            DrawPreviewBone(neckJoint, leftShoulder);
            DrawPreviewBone(leftShoulder, leftHand);

            DrawPreviewBone(neckJoint, rightShoulder);
            DrawPreviewBone(rightShoulder, rightHand);

            // Legs
            DrawPreviewBone(hipCenter, leftFoot);
            DrawPreviewBone(hipCenter, rightFoot);
        }

        // ============================================================
        // NAME
        // ============================================================

        if (Vars::ESP::nameMode != 0)
        {
            const char* previewName =
                Vars::ESP::nameMode == 1
                ? "Player_123"
                : "Shrimp";

            ImVec2 ts =
                ImGui::CalcTextSize(previewName);

            auto& nc = Vars::ESP::nameColor;

            ImU32 nameCol = IM_COL32(
                static_cast<int>(nc[0] * 255),
                static_cast<int>(nc[1] * 255),
                static_cast<int>(nc[2] * 255),
                static_cast<int>(nc[3] * 255 * alpha)
            );

            dl->AddText(
                ImVec2(
                    centerX - ts.x * 0.5f,
                    minY - ts.y - 4.0f
                ),
                nameCol,
                previewName
            );
        }

        // ============================================================
// TEAM NAME PREVIEW
// ============================================================

        if (Vars::ESP::teamNames)
        {
            const char* teamText = "Team1";

            ImVec2 ts =
                ImGui::CalcTextSize(teamText);

            auto& nc =
                Vars::ESP::nameColor;

            ImU32 teamCol =
                IM_COL32(
                    static_cast<int>(nc[0] * 255.0f),
                    static_cast<int>(nc[1] * 255.0f),
                    static_cast<int>(nc[2] * 255.0f),
                    static_cast<int>(nc[3] * 255.0f * alpha)
                );

            ImVec2 teamPos;

            if (Vars::ESP::teamNamePosition == 0)
            {
                teamPos =
                    ImVec2(
                        centerX - ts.x * 0.5f,
                        minY - 35.0f
                    );
            }
            else
            {
                teamPos =
                    ImVec2(
                        maxX + 7.0f,
                        minY +
                        (boxHeight * 0.5f) -
                        (ts.y * 0.5f)
                    );
            }

            dl->AddText(
                teamPos,
                teamCol,
                teamText
            );
        }

        // ============================================================
        // ANIMATED HEALTH BAR
        // ============================================================

        if (Vars::ESP::healthBar)
        {
            //
            // 0 -> 1 -> 0 continuously
            //
            float healthPct =
                (sinf(static_cast<float>(ImGui::GetTime()) * 1.35f)
                    + 1.0f) * 0.5f;

            // Keep a tiny amount visible
            healthPct =
                0.05f + healthPct * 0.95f;

            float barX = minX - 8.0f;

            // Black background
            dl->AddRectFilled(
                ImVec2(barX, minY),
                ImVec2(barX + 5.0f, maxY),
                IM_COL32(15, 15, 15, a)
            );

            dl->AddRect(
                ImVec2(barX, minY),
                ImVec2(barX + 5.0f, maxY),
                IM_COL32(0, 0, 0, a)
            );

            float hpY =
                maxY - boxHeight * healthPct;

            //
            // Health colour:
            //
            // 100% = green
            // 50%  = yellow
            // 0%   = red
            //
            int r;
            int g;

            if (healthPct > 0.5f)
            {
                float t =
                    (healthPct - 0.5f) / 0.5f;

                r = static_cast<int>(
                    255.0f * (1.0f - t)
                    );

                g = 255;
            }
            else
            {
                float t =
                    healthPct / 0.5f;

                r = 255;

                g = static_cast<int>(
                    255.0f * t
                    );
            }

            ImU32 healthCol =
                IM_COL32(r, g, 35, a);

            dl->AddRectFilled(
                ImVec2(barX + 1.0f, hpY),
                ImVec2(barX + 4.0f, maxY - 1.0f),
                healthCol
            );
        }

        // ============================================================
// HOVER PLAYER STATS - ESP PREVIEW
// ============================================================

        if (Vars::ESP::hoverStats)
        {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const float hoverPad = 8.0f;

            const bool hovered =
                mouse.x >= minX - hoverPad &&
                mouse.x <= maxX + hoverPad &&
                mouse.y >= minY - hoverPad &&
                mouse.y <= maxY + hoverPad;

            if (hovered)
            {
                // Preview/demo information
                const char* playerName = "Player";
                const char* teamText = "Enemy";

                const int hp = 85;
                const int maxHp = 100;

                const float hpPercent =
                    static_cast<float>(hp) /
                    static_cast<float>(maxHp);

                // ========================================================
                // CARD POSITION
                // ========================================================

                const float cardWidth = 205.0f;
                const float cardHeight = 88.0f;

                ImVec2 cardPos(
                    maxX + 13.0f,
                    minY + 10.0f
                );

                ImVec2 display = ImGui::GetIO().DisplaySize;

                // Flip to left side if it would leave the screen
                if (cardPos.x + cardWidth > display.x - 10.0f)
                {
                    cardPos.x =
                        minX - cardWidth - 13.0f;
                }

                if (cardPos.y + cardHeight > display.y - 10.0f)
                {
                    cardPos.y =
                        display.y - cardHeight - 10.0f;
                }

                ImVec2 cardEnd(
                    cardPos.x + cardWidth,
                    cardPos.y + cardHeight
                );

                // ========================================================
                // THEME COLOR
                // ========================================================

                ImVec4 theme(
                    Vars::Settings::themeColor[0],
                    Vars::Settings::themeColor[1],
                    Vars::Settings::themeColor[2],
                    1.0f
                );

                ImU32 accent =
                    ImGui::ColorConvertFloat4ToU32(theme);

                // ========================================================
                // SHADOW
                // ========================================================

                dl->AddRectFilled(
                    ImVec2(cardPos.x + 4.0f, cardPos.y + 5.0f),
                    ImVec2(cardEnd.x + 4.0f, cardEnd.y + 5.0f),
                    IM_COL32(0, 0, 0, 110),
                    8.0f
                );

                // ========================================================
                // BACKGROUND
                // ========================================================

                dl->AddRectFilled(
                    cardPos,
                    cardEnd,
                    IM_COL32(17, 17, 20, 245),
                    8.0f
                );

                dl->AddRect(
                    cardPos,
                    cardEnd,
                    IM_COL32(65, 65, 72, 220),
                    8.0f,
                    0,
                    1.0f
                );

                // ========================================================
                // LEFT THEME STRIP
                // ========================================================

                dl->AddRectFilled(
                    ImVec2(
                        cardPos.x,
                        cardPos.y + 8.0f
                    ),
                    ImVec2(
                        cardPos.x + 3.0f,
                        cardEnd.y - 8.0f
                    ),
                    accent,
                    2.0f
                );

                // ========================================================
                // PLAYER NAME
                // ========================================================

                dl->AddText(
                    ImVec2(
                        cardPos.x + 13.0f,
                        cardPos.y + 10.0f
                    ),
                    IM_COL32(245, 245, 248, 255),
                    playerName
                );

                // ========================================================
                // TEAM
                // ========================================================

                dl->AddText(
                    ImVec2(
                        cardPos.x + 13.0f,
                        cardPos.y + 29.0f
                    ),
                    IM_COL32(145, 145, 153, 255),
                    teamText
                );

                // ========================================================
                // HP TEXT
                // ========================================================

                char hpText[32];

                sprintf_s(
                    hpText,
                    "%d / %d HP",
                    hp,
                    maxHp
                );

                ImVec2 hpSize =
                    ImGui::CalcTextSize(hpText);

                dl->AddText(
                    ImVec2(
                        cardEnd.x - hpSize.x - 12.0f,
                        cardPos.y + 10.0f
                    ),
                    IM_COL32(210, 210, 215, 255),
                    hpText
                );

                // ========================================================
                // HEALTH BAR
                // ========================================================

                ImVec2 barMin(
                    cardPos.x + 13.0f,
                    cardPos.y + 55.0f
                );

                ImVec2 barMax(
                    cardEnd.x - 13.0f,
                    cardPos.y + 62.0f
                );

                // Empty/background bar
                dl->AddRectFilled(
                    barMin,
                    barMax,
                    IM_COL32(42, 42, 47, 255),
                    3.0f
                );

                // Filled HP bar
                ImVec2 hpEnd(
                    barMin.x +
                    (barMax.x - barMin.x) * hpPercent,
                    barMax.y
                );

                dl->AddRectFilled(
                    barMin,
                    hpEnd,
                    accent,
                    3.0f
                );

                // ========================================================
                // FOOTER
                // ========================================================

                dl->AddText(
                    ImVec2(
                        cardPos.x + 13.0f,
                        cardPos.y + 69.0f
                    ),
                    IM_COL32(125, 125, 133, 255),
                    "Hover Preview"
                );
            }
        }

        // ============================================================
        // BOTTOM ESP TEXT
        // ============================================================

        float bottomY = maxY + 5.0f;

        if (Vars::ESP::distance)
        {
            const char* text = "23m";

            ImVec2 ts =
                ImGui::CalcTextSize(text);

            auto& dc =
                Vars::ESP::distanceColor;

            dl->AddText(
                ImVec2(
                    centerX - ts.x * 0.5f,
                    bottomY
                ),
                IM_COL32(
                    static_cast<int>(dc[0] * 255),
                    static_cast<int>(dc[1] * 255),
                    static_cast<int>(dc[2] * 255),
                    static_cast<int>(dc[3] * 255 * alpha)
                ),
                text
            );

            bottomY += ts.y + 3.0f;
        }

        if (Vars::ESP::toolName)
        {
            const char* text = "Tool";

            ImVec2 ts =
                ImGui::CalcTextSize(text);

            auto& tc =
                Vars::ESP::toolNameColor;

            dl->AddText(
                ImVec2(
                    centerX - ts.x * 0.5f,
                    bottomY
                ),
                IM_COL32(
                    static_cast<int>(tc[0] * 255),
                    static_cast<int>(tc[1] * 255),
                    static_cast<int>(tc[2] * 255),
                    static_cast<int>(tc[3] * 255 * alpha)
                ),
                text
            );
        }

        ImGui::End();

        ImGui::PopStyleVar(3);
    }

    struct MenuParticle
    {
        ImVec2 pos;
        ImVec2 vel;
        float radius;
        float alpha;
    };

    void RenderAimbotFOV()
    {
        if (!Vars::Aimbot::showFOV)
            return;

        ImGuiIO& io = ImGui::GetIO();

        // Your aimbot uses current mouse position as its FOV center
        POINT mousePos;
        GetCursorPos(&mousePos);

        ImVec2 center(
            static_cast<float>(mousePos.x),
            static_cast<float>(mousePos.y)
        );

        auto& c = Vars::Aimbot::fovColor;

        ImU32 outlineColor =
            IM_COL32(
                static_cast<int>(c[0] * 255.0f),
                static_cast<int>(c[1] * 255.0f),
                static_cast<int>(c[2] * 255.0f),
                static_cast<int>(c[3] * 255.0f)
            );

        ImDrawList* drawList =
            ImGui::GetBackgroundDrawList();

        // Transparent fill
        if (Vars::Aimbot::fovFilled)
        {
            float alpha =
                Vars::Aimbot::fovFillAlpha;

            if (alpha < 0.0f)
                alpha = 0.0f;

            if (alpha > 1.0f)
                alpha = 1.0f;

            ImU32 fillColor =
                IM_COL32(
                    static_cast<int>(c[0] * 255.0f),
                    static_cast<int>(c[1] * 255.0f),
                    static_cast<int>(c[2] * 255.0f),
                    static_cast<int>(alpha * 255.0f)
                );

            drawList->AddCircleFilled(
                center,
                Vars::Aimbot::fovRadius,
                fillColor,
                96
            );
        }

        // Main FOV outline
        drawList->AddCircle(
            center,
            Vars::Aimbot::fovRadius,
            outlineColor,
            96,
            1.5f
        );
    }



    void RenderMenuParticles(float menuAlpha)
    {
        if (menuAlpha <= 0.01f)
            return;

        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* bg = ImGui::GetBackgroundDrawList();

        static std::vector<MenuParticle> particles;
        static bool initialized = false;

        const int particleCount = 70;

        if (!initialized)
        {
            particles.reserve(particleCount);

            for (int i = 0; i < particleCount; i++)
            {
                MenuParticle p{};

                p.pos = ImVec2(
                    static_cast<float>(rand() % static_cast<int>(io.DisplaySize.x)),
                    static_cast<float>(rand() % static_cast<int>(io.DisplaySize.y))
                );

                p.vel = ImVec2(
                    ((rand() % 100) / 100.0f - 0.5f) * 10.0f,
                    ((rand() % 100) / 100.0f - 0.5f) * 10.0f
                );

                p.radius =
                    1.0f + (rand() % 100) / 100.0f * 1.5f;

                p.alpha =
                    0.20f + (rand() % 100) / 100.0f * 0.35f;

                particles.push_back(p);
            }

            initialized = true;
        }

        float dt = io.DeltaTime;

        ImVec4 accent =
            ImGui::ColorConvertU32ToFloat4(Menu::Accent);

        for (auto& p : particles)
        {
            p.pos.x += p.vel.x * dt;
            p.pos.y += p.vel.y * dt;

            // Wrap around screen
            if (p.pos.x < -10.0f)
                p.pos.x = io.DisplaySize.x + 10.0f;

            if (p.pos.x > io.DisplaySize.x + 10.0f)
                p.pos.x = -10.0f;

            if (p.pos.y < -10.0f)
                p.pos.y = io.DisplaySize.y + 10.0f;

            if (p.pos.y > io.DisplaySize.y + 10.0f)
                p.pos.y = -10.0f;

            int alpha =
                static_cast<int>(
                    255.0f *
                    p.alpha *
                    menuAlpha
                    );

            ImU32 col = IM_COL32(
                static_cast<int>(accent.x * 255.0f),
                static_cast<int>(accent.y * 255.0f),
                static_cast<int>(accent.z * 255.0f),
                alpha
            );

            bg->AddCircleFilled(
                p.pos,
                p.radius,
                col,
                12
            );
        }
    }



    void RenderMenu()
    {
        static float menuAlpha = 0.0f;

        // How fast the menu fades
        float fadeSpeed = 8.0f;

        float targetAlpha = Vars::menuOpen ? 1.0f : 0.0f;

        float step = ImGui::GetIO().DeltaTime * fadeSpeed;

        if (step > 1.0f)
            step = 1.0f;

        menuAlpha += (targetAlpha - menuAlpha) * step;

        // Snap when extremely close
        if (menuAlpha < 0.01f && !Vars::menuOpen)
        {
            menuAlpha = 0.0f;
            return;
        }

        if (menuAlpha > 0.99f && Vars::menuOpen)
            menuAlpha = 1.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, menuAlpha);

        int menuAlpha255 = static_cast<int>(menuAlpha * 255.0f);

        if (menuAlpha255 < 0)
            menuAlpha255 = 0;

        if (menuAlpha255 > 255)
            menuAlpha255 = 255;

        ImGuiStyle& Style = ImGui::GetStyle();
        ImGuiIO& Io = ImGui::GetIO(); (void)Io;

        // ==========================================================
// DARK BACKGROUND
// ==========================================================
        if (Vars::Settings::darkBackground)
        {
            int darkAlpha = static_cast<int>(210.0f * menuAlpha);

            ImGui::GetBackgroundDrawList()->AddRectFilled(
                ImVec2(0.0f, 0.0f),
                Io.DisplaySize,
                IM_COL32(0, 0, 0, darkAlpha)
            );
        }

        if (Vars::Settings::menuParticles)
        {
            RenderMenuParticles(menuAlpha);
        }

        // ==========================================================
        // DYNAMIC MENU THEME
        // ==========================================================

        ImVec4 theme(
            Vars::Settings::themeColor[0],
            Vars::Settings::themeColor[1],
            Vars::Settings::themeColor[2],
            1.0f
        );

        Menu::Accent = ImGui::ColorConvertFloat4ToU32(theme);

        ImVec4 darkTheme(
            theme.x * 0.60f,
            theme.y * 0.60f,
            theme.z * 0.60f,
            1.0f
        );

        Menu::DarkAccent = ImGui::ColorConvertFloat4ToU32(darkTheme);

        Style.Colors[ImGuiCol_CheckMark] = theme;
        Style.Colors[ImGuiCol_SliderGrab] = theme;
        Style.Colors[ImGuiCol_Header] = theme;
        Style.Colors[ImGuiCol_SeparatorHovered] = theme;

        Style.Colors[ImGuiCol_SliderGrabActive] = darkTheme;
        Style.Colors[ImGuiCol_HeaderActive] = darkTheme;
        Style.Colors[ImGuiCol_SeparatorActive] = darkTheme;


        // EXISTING CODE CONTINUES
        ImGui::SetNextWindowSize(ImVec2(700, 900), ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2(Io.DisplaySize.x / 2.0f, Io.DisplaySize.y / 2.0f), ImGuiCond_Once, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        float HeaderHeight = 70.0f;

        bool MainWindow = ImGui::Begin(
            "main",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoBackground
        );
        ImGui::PopStyleVar(2);

        if (MainWindow)
        {
            ImVec2 WinPos = ImGui::GetWindowPos();
            ImVec2 WinSize = ImGui::GetWindowSize();
            ImDrawList* DrawList = ImGui::GetWindowDrawList();

            DrawList->AddRectFilled(
                WinPos,
                WinPos + WinSize,
                IM_COL32(12, 12, 12, menuAlpha255),
                6.0f
            );

            //DrawList->AddRect(
            //    WinPos + ImVec2(1.0f, 1.0f),
            //    WinPos + WinSize - ImVec2(1.0f, 1.0f),
            //    IM_COL32(45, 45, 45, menuAlpha255)
            //);

            //DrawList->AddRect(
             //   WinPos + ImVec2(2.0f, 2.0f),
             //   WinPos + WinSize - ImVec2(2.0f, 2.0f),
             //   IM_COL32(220, 25, 30, menuAlpha255)
           // );

            float Top = HeaderHeight + Style.WindowBorderSize - 8.0f;
            float Bottom = Style.WindowBorderSize + Style.WindowPadding.y + 1.0f;

            ImVec2 ContentMin = WinPos + ImVec2(10.0f, 75.0f);
            ImVec2 ContentMax = WinPos + ImVec2(WinSize.x - 10.0f, WinSize.y - 10.0f);

            DrawList->AddRectFilled(
                WinPos,
                WinPos + WinSize,
                IM_COL32(12, 12, 12, menuAlpha255),
                6.0f
            );

            DrawList->AddRect(
                ContentMin,
                ContentMax,
                IM_COL32(45, 45, 45, menuAlpha255),
                6.0f,
                0,
                1.0f
            );

            // ==========================================================
// TITLE - own row
// ==========================================================

            ImVec2 titlePos(
                WinPos.x + 12.0f,
                WinPos.y + 9.0f
            );

            Menu::DrawLabelShadow(
                DrawList,
                titlePos,
                Menu::Accent,
                "Shrimplock"
            );

            titlePos.x += ImGui::CalcTextSize("Shrimplock").x;

            Menu::DrawLabelShadow(
                DrawList,
                titlePos,
                Menu::Text,
                " External"
            );

            const char* tabs[] = {
    "Aiming",
    "Visuals",
    "Player",
    "Rage",
    "Misc",
    "Settings"
            };

            const int tabIds[] = {
                0, 1, 3, 4, 2, 5,
            };

            float buttonWidth = 105.0f;
            float buttonHeight = 28.0f;
            float spacing = 6.0f;

            float totalWidth = buttonWidth * 6 + spacing * 5;

            float tabX = WinPos.x + (WinSize.x - totalWidth) * 0.5f;
            float tabY = WinPos.y + 35.0f;

            static float tabHoverAnim[6] = { 0.0f };

            for (int i = 0; i < 6; i++)
            {
                ImVec2 buttonPos(tabX, tabY);
                ImVec2 buttonSize(105.0f, 28.0f);

                ImGui::SetCursorScreenPos(buttonPos);

                bool selected = Vars::selectedTab == tabIds[i];

                // Keep background the exact same color on normal/hover/click
                ImVec4 tabBg = ImVec4(0.047f, 0.047f, 0.047f, 1.0f);

                ImGui::PushStyleColor(ImGuiCol_Button, tabBg);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, tabBg);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, tabBg);

                if (ImGui::Button(tabs[i], buttonSize))
                {
                    Vars::selectedTab = tabIds[i];
                }

                bool hovered = ImGui::IsItemHovered();

                ImGui::PopStyleColor(3);

                // Smooth fade in/out
                float targetAlpha = (hovered && !selected) ? 1.0f : 0.0f;

                float fadeSpeed = 8.0f;
                float step = ImGui::GetIO().DeltaTime * fadeSpeed;

                if (step > 1.0f)
                    step = 1.0f;

                tabHoverAnim[i] +=
                    (targetAlpha - tabHoverAnim[i]) * step;

                // Prevent tiny leftover floating-point values
                if (tabHoverAnim[i] < 0.001f)
                    tabHoverAnim[i] = 0.0f;

                ImDrawList* drawList = ImGui::GetWindowDrawList();

                ImVec2 underlineMin(
                    buttonPos.x,
                    buttonPos.y + buttonSize.y - 2.0f
                );

                ImVec2 underlineMax(
                    buttonPos.x + buttonSize.x,
                    buttonPos.y + buttonSize.y
                );

                // Selected tab = permanent full red
                if (selected)
                {
                    ImVec4 accent = ImGui::ColorConvertU32ToFloat4(Menu::Accent);
                    accent.w *= menuAlpha;

                    drawList->AddRectFilled(
                        underlineMin,
                        underlineMax,
                        ImGui::ColorConvertFloat4ToU32(accent)
                    );
                }
                // Non-selected tab = animated faded red
                else if (tabHoverAnim[i] > 0.0f)
                {
                    int alpha = static_cast<int>(
                        180.0f * tabHoverAnim[i]
                        );

                    ImVec4 hoverAccent =
                        ImGui::ColorConvertU32ToFloat4(Menu::Accent);

                    hoverAccent.w *= menuAlpha * tabHoverAnim[i] * 0.70f;

                    drawList->AddRectFilled(
                        underlineMin,
                        underlineMax,
                        ImGui::ColorConvertFloat4ToU32(hoverAccent)
                    );
                }
                    
                tabX += buttonWidth + spacing;
            }

            // const char* TabLabels[] = { "Aiming", "Visuals", "World", "Player", "Rage", "Settings" };
            // const int TabCount = IM_ARRAYSIZE(TabLabels);

            // Menu::Tabs(DrawList, WinPos, TextY, Vars::selectedTab, TabLabels, TabCount, 10.0f, 8.0f);

            ImGui::SetCursorScreenPos(ContentMin);
            ImGui::PushClipRect(ContentMin, ContentMax, true);
            ImGui::BeginGroup();

            ImVec2 AvailSize(ContentMax.x - ContentMin.x, ContentMax.y - ContentMin.y);
            float Spacing = Style.ItemSpacing.x;
            float LeftWidth = (AvailSize.x - Spacing) * 0.5f;
            float RightWidth = AvailSize.x - Spacing - LeftWidth;
            float SidePad = 3.0f;
            float HalfHeight = (AvailSize.y - SidePad * 2.0f) * 0.5f;

            if (Vars::selectedTab == 0) // Aiming
            {
                float startY = ImGui::GetCursorPosY(); // Top of the group 
                float rightColX = LeftWidth + Spacing; // X position for right column

                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SidePad);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SidePad);

                if (Menu::BeginChild(
                    "Aimbot",
                    ImVec2(LeftWidth - SidePad, HalfHeight)
                ))
                {
                    Menu::CheckBox(
                        "Enabled",
                        &Vars::Aimbot::enabled
                    );

                    // ORIGINAL TARGET HIT PARTS
                    std::vector<const char*> targets =
                    {
                        "Head",
                        "HumanoidRootPart",
                        "Left Arm",
                        "Right Arm",
                        "Left Leg",
                        "Right Leg",
                        "Random"
                    };

                    Menu::CheckBox(
                        "Random Targeting",
                        &Vars::Aimbot::randomTarget
                    );

                    // ORIGINAL AIM METHODS
                    std::vector<const char*> methods =
                    {
                        "Mouse Move",
                        "Camera Rotation"
                    };

                    ImGui::Text("Aim Target");
                    ImGui::SetNextItemWidth(
                        ImGui::GetContentRegionAvail().x
                    );

                    ImGui::Combo(
                        "##aim_target",
                        &Vars::Aimbot::aimTarget,
                        targets.data(),
                        static_cast<int>(targets.size())
                    );

                    ImGui::Text("Aim Method");
                    ImGui::SetNextItemWidth(
                        ImGui::GetContentRegionAvail().x
                    );

                    ImGui::Combo(
                        "##aim_method",
                        &Vars::Aimbot::aimMethod,
                        methods.data(),
                        static_cast<int>(methods.size())
                    );

                    HotkeyButton(
                        "Aimbot Keybind",
                        &Vars::Aimbot::aimbotKey
                    );

                    HotkeyButton(
                        "Toggle Aimbot",
                        &Vars::Aimbot::toggleKey
                    );

                    Menu::EndChild();
                }
                
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SidePad);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SidePad);

                // ============================================================
// AIMBOT SETTINGS / TEAM WHITELIST SUB-TABS
// ============================================================

                static int aimingSubTab = 0; // 0 = Aimbot, 1 = Team Whitelist
                static float aimingTabAnim[2] = { 0.0f, 0.0f };

                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SidePad);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SidePad);

                float TeamWhitelistHeight = HalfHeight - 8;

                if (Menu::BeginChild(
                    "##AimbotSettingsContainer",
                    ImVec2(LeftWidth - SidePad, TeamWhitelistHeight)
                ))
                {
                    const char* subTabs[] =
                    {
                        "Aimbot Settings",
                        "Team Whitelist"
                    };

                    const int subTabCount = IM_ARRAYSIZE(subTabs);

                    float tabHeight = 27.0f;
                    float tabSpacing = 5.0f;

                    float availableWidth = ImGui::GetContentRegionAvail().x;
                    float tabWidth =
                        (availableWidth - tabSpacing * (subTabCount - 1)) /
                        static_cast<float>(subTabCount);

                    ImDrawList* drawList = ImGui::GetWindowDrawList();

                    for (int i = 0; i < subTabCount; ++i)
                    {
                        if (i > 0)
                            ImGui::SameLine(0.0f, tabSpacing);

                        ImVec2 buttonMin = ImGui::GetCursorScreenPos();
                        ImVec2 buttonSize(tabWidth, tabHeight);
                        ImVec2 buttonMax(
                            buttonMin.x + buttonSize.x,
                            buttonMin.y + buttonSize.y
                        );

                        bool selected = (aimingSubTab == i);

                        // Invisible actual ImGui button.
                        ImGui::PushStyleColor(
                            ImGuiCol_Button,
                            ImVec4(0.0f, 0.0f, 0.0f, 0.0f)
                        );

                        ImGui::PushStyleColor(
                            ImGuiCol_ButtonHovered,
                            ImVec4(0.0f, 0.0f, 0.0f, 0.0f)
                        );

                        ImGui::PushStyleColor(
                            ImGuiCol_ButtonActive,
                            ImVec4(0.0f, 0.0f, 0.0f, 0.0f)
                        );

                        ImGui::PushStyleVar(
                            ImGuiStyleVar_FrameRounding,
                            5.0f
                        );

                        if (ImGui::Button(
                            (std::string(subTabs[i]) + "##AimingSubTab" +
                                std::to_string(i)).c_str(),
                            buttonSize
                        ))
                        {
                            aimingSubTab = i;
                        }

                        bool hovered = ImGui::IsItemHovered();

                        ImGui::PopStyleVar();
                        ImGui::PopStyleColor(3);

                        // ========================================================
                        // SMOOTH FADE
                        // ========================================================

                        float target =
                            (selected || hovered)
                            ? 1.0f
                            : 0.0f;

                        float speed = 9.0f;

                        aimingTabAnim[i] +=
                            (target - aimingTabAnim[i]) *
                            ImGui::GetIO().DeltaTime *
                            speed;

                        if (aimingTabAnim[i] < 0.0f)
                            aimingTabAnim[i] = 0.0f;

                        if (aimingTabAnim[i] > 1.0f)
                            aimingTabAnim[i] = 1.0f;

                        // ========================================================
                        // WHITE / GREY TRANSPARENT BACKGROUND
                        // ========================================================

                        if (aimingTabAnim[i] > 0.001f)
                        {
                            float baseAlpha =
                                selected
                                ? 0.14f
                                : 0.08f;

                            ImVec4 bgColor(
                                0.82f,
                                0.82f,
                                0.84f,
                                baseAlpha * aimingTabAnim[i] * menuAlpha
                            );

                            // Button was already submitted, so draw behind-ish
                            // with the window draw list.
                            drawList->AddRectFilled(
                                buttonMin,
                                buttonMax,
                                ImGui::ColorConvertFloat4ToU32(bgColor),
                                5.0f
                            );

                            // Redraw text above our custom background.
                            ImVec2 textSize =
                                ImGui::CalcTextSize(subTabs[i]);

                            ImVec2 textPos(
                                buttonMin.x +
                                (buttonSize.x - textSize.x) * 0.5f,

                                buttonMin.y +
                                (buttonSize.y - textSize.y) * 0.5f
                            );

                            ImVec4 textColor =
                                selected
                                ? ImVec4(0.93f, 0.93f, 0.95f, menuAlpha)
                                : ImVec4(0.72f, 0.72f, 0.74f, menuAlpha);

                            drawList->AddText(
                                textPos,
                                ImGui::ColorConvertFloat4ToU32(textColor),
                                subTabs[i]
                            );
                        }
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // ============================================================
                    // AIMBOT TAB
                    // ============================================================

                    if (aimingSubTab == 0)
                    {
                        Menu::CheckBox(
                            "Sticky Aim",
                            &Vars::Aimbot::stickyAim
                        );

                        Menu::CheckBox(
                            "Team Check",
                            &Vars::Aimbot::teamCheck
                        );

                        Menu::CheckBox(
                            "NPC Check",
                            &Vars::Aimbot::npcCheck
                        );

                        Menu::CheckBox(
                            "Downed Check",
                            &Vars::Aimbot::downedCheck
                        );

                        Menu::CheckBox(
                            "Invis Check",
                            &Vars::Aimbot::invisCheck
                        );

                        Menu::CheckBox(
                            "ForceField Check",
                            &Vars::Aimbot::invincibleCheck
                        );

                        Menu::CheckBox(
                            "Show FOV Circle",
                            &Vars::Aimbot::showFOV
                        );

                        // FOV color picker on the same row
                        ImGui::SameLine();

                        float fovPickerX =
                            ImGui::GetWindowWidth() -
                            ImGui::GetStyle().WindowPadding.x -
                            24.0f;

                        ImGui::SetCursorPosX(fovPickerX);

                        ImGui::ColorEdit4(
                            "##AimbotFOVColor",
                            Vars::Aimbot::fovColor,
                            ImGuiColorEditFlags_NoInputs |
                            ImGuiColorEditFlags_NoLabel |
                            ImGuiColorEditFlags_AlphaBar
                        );

                        if (Vars::Aimbot::showFOV)
                        {
                            Menu::CheckBox(
                                "FOV Filling",
                                &Vars::Aimbot::fovFilled
                            );

                            if (Vars::Aimbot::fovFilled)
                            {
                                Menu::SliderFloat(
                                    "Fill Opacity",
                                    &Vars::Aimbot::fovFillAlpha,
                                    0.01f,
                                    0.50f,
                                    "%.2f"
                                );
                            }

                            Menu::SliderFloat(
                                "FOV Radius",
                                &Vars::Aimbot::fovRadius,
                                10.0f,
                                500.0f,
                                "%.0f"
                            );
                        }

                        Menu::SliderFloat(
                            "Smoothing",
                            &Vars::Aimbot::smoothing,
                            1.0f,
                            20.0f,
                            "%.1f"
                        );

                        Menu::CheckBox(
                            "Prediction",
                            &Vars::Aimbot::prediction
                        );

                        if (Vars::Aimbot::prediction)
                        {
                            Menu::SliderFloat(
                                "Prediction X",
                                &Vars::Aimbot::predX,
                                0.0f,
                                2.1f,
                                "%.2f"
                            );

                            Menu::SliderFloat(
                                "Prediction Y",
                                &Vars::Aimbot::predY,
                                0.0f,
                                2.1f,
                                "%.2f"
                            );
                        }
                    }

                    // ============================================================
                    // TEAM WHITELIST TAB
                    // ============================================================

                    else if (aimingSubTab == 1)
                    {
                        Menu::CheckBox(
                            "Enable Team Whitelist",
                            &Vars::Aimbot::teamWhitelistEnabled
                        );

                        ImGui::Spacing();

                        ImGui::TextDisabled(
                            "Whitelisted teams are ignored by Aimbot"
                        );

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        auto snapshot = PlayerCache::GetSnapshot();

                        std::vector<uintptr_t> shownTeams;

                        bool foundTeam = false;

                        for (auto& p : snapshot)
                        {
                            if (p.teamAddr == 0 || p.teamName.empty())
                                continue;

                            if (std::find(
                                shownTeams.begin(),
                                shownTeams.end(),
                                p.teamAddr
                            ) != shownTeams.end())
                            {
                                continue;
                            }

                            shownTeams.push_back(p.teamAddr);
                            foundTeam = true;

                            bool whitelisted =
                                std::find(
                                    Vars::Aimbot::whitelistedTeams.begin(),
                                    Vars::Aimbot::whitelistedTeams.end(),
                                    p.teamAddr
                                )
                                != Vars::Aimbot::whitelistedTeams.end();

                            ImGui::PushID(
                                reinterpret_cast<void*>(p.teamAddr)
                            );

                            if (Menu::CheckBox(
                                p.teamName.c_str(),
                                &whitelisted
                            ))
                            {
                                if (whitelisted)
                                {
                                    if (std::find(
                                        Vars::Aimbot::whitelistedTeams.begin(),
                                        Vars::Aimbot::whitelistedTeams.end(),
                                        p.teamAddr
                                    ) == Vars::Aimbot::whitelistedTeams.end())
                                    {
                                        Vars::Aimbot::whitelistedTeams.push_back(
                                            p.teamAddr
                                        );
                                    }
                                }
                                else
                                {
                                    Vars::Aimbot::whitelistedTeams.erase(
                                        std::remove(
                                            Vars::Aimbot::whitelistedTeams.begin(),
                                            Vars::Aimbot::whitelistedTeams.end(),
                                            p.teamAddr
                                        ),
                                        Vars::Aimbot::whitelistedTeams.end()
                                    );
                                }
                            }

                            ImGui::PopID();
                        }

                        if (!foundTeam)
                        {
                            ImGui::TextDisabled(
                                "No teams found in the current game."
                            );
                        }
                    }

                    Menu::EndChild();
                }
                
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SidePad);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SidePad);
                float AimbotSettingsHeight = HalfHeight - 8;

                if (Menu::BeginChild(
                    "Aimbot Settings",
                    ImVec2(LeftWidth - SidePad, AimbotSettingsHeight)
                ))
                {
                    Menu::CheckBox("Team Check", &Vars::Aimbot::teamCheck);
                    Menu::CheckBox("NPC Check", &Vars::Aimbot::npcCheck);
                    Menu::CheckBox("Downed Check", &Vars::Aimbot::downedCheck);
                    Menu::CheckBox("Invis Check", &Vars::Aimbot::invisCheck);
                    Menu::CheckBox("ForceField Check", &Vars::Aimbot::invincibleCheck);

                    Menu::CheckBox("Show FOV Circle", &Vars::Aimbot::showFOV);
                    Menu::SliderFloat("FOV Radius", &Vars::Aimbot::fovRadius, 10.0f, 500.0f, "%.0f");
                    Menu::SliderFloat("Smoothing", &Vars::Aimbot::smoothing, 1.0f, 20.0f, "%.1f");
                    
                    Menu::CheckBox("Prediction", &Vars::Aimbot::prediction);
                    if (Vars::Aimbot::prediction) {
                        Menu::SliderFloat("Prediction X", &Vars::Aimbot::predX, 0.0f, 2.1f, "%.2f");
                        Menu::SliderFloat("Prediction Y", &Vars::Aimbot::predY, 0.0f, 2.1f, "%.2f");
                    }

                    int64_t placeId = 0;
                    if (Globals::dataModel.Addr != 0) {
                        placeId = Coms->ReadMemory<int64_t>(Globals::dataModel.Addr + Offsets::DataModel::PlaceId);
                    }
                    
                    Menu::EndChild();
                }

                float rightX = ContentMin.x + LeftWidth + Spacing;
                float topY = ContentMin.y + SidePad;

                ImGui::SetCursorScreenPos(ImVec2(rightX, topY));
                float SilentH = AvailSize.y * 0.40f;

                if (Menu::BeginChild("Silent", ImVec2(RightWidth - SidePad, SilentH)))
                {
                    Menu::CheckBox("Silent Aim", &Vars::Aimbot::silentAim);
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - Menu::GetColorPickerWidth() + Style.WindowPadding.x + 12.0f);
                    Menu::KeyBind("Silent Keybind", &Vars::Aimbot::silentAimbotKey);

                    Menu::CheckBox("Silent Prediction", &Vars::Aimbot::silentPrediction);
                    Menu::CheckBox("Show Silent FOV", &Vars::Aimbot::showSilentFOV);
                    Menu::SliderFloat("Silent FOV", &Vars::Aimbot::silentFOV, 10.0f, 500.0f, "%.0f");

                    Menu::EndChild();
                }

                float triggerY = topY + SilentH + SidePad;

                ImGui::SetCursorScreenPos(
                    ImVec2(rightX, triggerY)
                );
                if (Menu::BeginChild("Triggerbot", ImVec2(RightWidth - SidePad, AvailSize.y - SilentH - SidePad * 3.0f)))
                {
                    Menu::CheckBox("Triggerbot", &Vars::Triggerbot::enabled);
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - Menu::GetColorPickerWidth() + Style.WindowPadding.x + 12.0f);
                    Menu::KeyBind("Trigger Key", &Vars::Triggerbot::key);

                    Menu::SliderFloat("Trigger FOV", &Vars::Triggerbot::fov, 1.0f, 200.0f, "%.0fpx");
                    Menu::SliderFloat("Fire Delay", &Vars::Triggerbot::delay, 0.0f, 300.0f, "%.0fms");
                    Menu::SliderFloat("Hitbox Scale", &Vars::Triggerbot::hitbox_scale, 0.1f, 10.0f, "%.1f");
                    Menu::SliderFloat("Fire Interval", &Vars::Triggerbot::interval, 10.0f, 1000.0f, "%.0fms");
                    
                    ImGui::Text("Target Hitboxes");
                    Menu::CheckBox("Head", &Vars::Triggerbot::hitboxes[0]);
                    Menu::CheckBox("Torso", &Vars::Triggerbot::hitboxes[1]);
                    Menu::CheckBox("Arms", &Vars::Triggerbot::hitboxes[2]);
                    Menu::CheckBox("Legs", &Vars::Triggerbot::hitboxes[3]);

                    Menu::CheckBox("Team Check##tb", &Vars::Triggerbot::teamCheck);

                    Menu::EndChild();
                }
            }
            else if (Vars::selectedTab == 1) // Visuals
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SidePad);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SidePad);

                static int visualsSubTab = 0;
                // 0 = ESP
                // 1 = Hitbox
                // 2 = Team Whitelist

                static float visualsTabAnim[3] =
                {
                    0.0f,
                    0.0f,
                    0.0f
                };

                if (Menu::BeginChild(
                    "ESP Visuals",
                    ImVec2(LeftWidth - SidePad, AvailSize.y - SidePad * 2.0f)
                ))
                {
                    const char* visualTabs[] =
                    {
                        "ESP",
                        "Hitbox",
                        "Team Whitelist"
                    };

                    const int visualTabCount = IM_ARRAYSIZE(visualTabs);

                    float tabHeight = 27.0f;
                    float tabSpacing = 5.0f;

                    float availableWidth = ImGui::GetContentRegionAvail().x;
                    float tabWidth =
                        (availableWidth -
                            tabSpacing * (visualTabCount - 1)) /
                        static_cast<float>(visualTabCount);

                    ImDrawList* drawList = ImGui::GetWindowDrawList();

                    for (int i = 0; i < visualTabCount; ++i)
                    {
                        if (i > 0)
                            ImGui::SameLine(0.0f, tabSpacing);

                        ImVec2 buttonMin = ImGui::GetCursorScreenPos();
                        ImVec2 buttonSize(tabWidth, tabHeight);

                        ImVec2 buttonMax(
                            buttonMin.x + buttonSize.x,
                            buttonMin.y + buttonSize.y
                        );

                        bool selected = visualsSubTab == i;

                        ImGui::PushStyleColor(
                            ImGuiCol_Button,
                            ImVec4(0, 0, 0, 0)
                        );

                        ImGui::PushStyleColor(
                            ImGuiCol_ButtonHovered,
                            ImVec4(0, 0, 0, 0)
                        );

                        ImGui::PushStyleColor(
                            ImGuiCol_ButtonActive,
                            ImVec4(0, 0, 0, 0)
                        );

                        ImGui::PushStyleVar(
                            ImGuiStyleVar_FrameRounding,
                            5.0f
                        );

                        if (ImGui::Button(
                            (
                                std::string(visualTabs[i]) +
                                "##VisualSubTab" +
                                std::to_string(i)
                                ).c_str(),
                            buttonSize
                        ))
                        {
                            visualsSubTab = i;
                        }

                        bool hovered = ImGui::IsItemHovered();

                        ImGui::PopStyleVar();
                        ImGui::PopStyleColor(3);

                        float target =
                            (selected || hovered)
                            ? 1.0f
                            : 0.0f;

                        float speed = 9.0f;

                        visualsTabAnim[i] +=
                            (target - visualsTabAnim[i]) *
                            ImGui::GetIO().DeltaTime *
                            speed;

                        if (visualsTabAnim[i] < 0.0f)
                            visualsTabAnim[i] = 0.0f;

                        if (visualsTabAnim[i] > 1.0f)
                            visualsTabAnim[i] = 1.0f;

                        if (visualsTabAnim[i] > 0.001f)
                        {
                            float baseAlpha =
                                selected
                                ? 0.14f
                                : 0.08f;

                            ImVec4 bgColor(
                                0.82f,
                                0.82f,
                                0.84f,
                                baseAlpha *
                                visualsTabAnim[i] *
                                menuAlpha
                            );

                            drawList->AddRectFilled(
                                buttonMin,
                                buttonMax,
                                ImGui::ColorConvertFloat4ToU32(bgColor),
                                5.0f
                            );

                            ImVec2 textSize =
                                ImGui::CalcTextSize(visualTabs[i]);

                            ImVec2 textPos(
                                buttonMin.x +
                                (buttonSize.x - textSize.x) * 0.5f,

                                buttonMin.y +
                                (buttonSize.y - textSize.y) * 0.5f
                            );

                            ImVec4 textColor =
                                selected
                                ? ImVec4(
                                    0.93f,
                                    0.93f,
                                    0.95f,
                                    menuAlpha
                                )
                                : ImVec4(
                                    0.72f,
                                    0.72f,
                                    0.74f,
                                    menuAlpha
                                );

                            drawList->AddText(
                                textPos,
                                ImGui::ColorConvertFloat4ToU32(textColor),
                                visualTabs[i]
                            );
                        }
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // ============================================================
                    // ESP TAB
                    // ============================================================

                    if (visualsSubTab == 0)
                    {
                        Menu::CheckBox("Enable ESP", &Vars::ESP::enabled);
                        Menu::CheckBox(
                            "Hover Player Stats",
                            &Vars::ESP::hoverStats
                        );
                        Menu::CheckBox("Dead Check", &Vars::ESP::deadCheck);
                        Menu::CheckBox("Team Check", &Vars::ESP::teamCheck);
                        Menu::CheckBox(
                            "Team Names",
                            &Vars::ESP::teamNames
                        );

                        if (Vars::ESP::teamNames)
                        {
                            std::vector<const char*> teamNamePositions =
                            {
                                "Above Head",
                                "Right Side"
                            };

                            Menu::Combo(
                                "Team Name Position",
                                &Vars::ESP::teamNamePosition,
                                teamNamePositions
                            );
                        }

                        Menu::CheckBox(
                            "NPC Check",
                            &Vars::ESP::npcCheck
                        );

                        std::vector<const char*> boxModes =
                        {
                            "Off",
                            "2D Box",
                            "Corner Box",
                            "3D Box"
                        };

                        Menu::Combo(
                            "Box Mode",
                            &Vars::ESP::boxMode,
                            boxModes
                        );

                        if (Vars::ESP::boxMode != 0)
                        {
                            if (Vars::ESP::boxMode != 3)
                            {
                                Menu::CheckBox(
                                    "Static Box Mode",
                                    &Vars::ESP::staticBox
                                );
                            }
                            else
                            {
                                // Real 3D box must use world-space geometry.
                                Vars::ESP::staticBox = false;
                            }

                            Menu::CheckBox(
                                "Rainbow RGB",
                                &Vars::ESP::rainbowBox
                            );

                            if (Vars::ESP::rainbowBox)
                            {
                                Menu::SliderFloat(
                                    "Rainbow Speed",
                                    &Vars::ESP::rainbowSpeed,
                                    0.05f,
                                    1.0f,
                                    "%.2f"
                                );
                            }

                            Menu::CheckBox(
                                "Gradient Mode",
                                &Vars::ESP::boxGradient
                            );

                            ImGui::SameLine();
                            ImGui::SetCursorPosX(
                                ImGui::GetWindowWidth() -
                                ImGui::GetStyle().WindowPadding.x -
                                24.0f
                            );

                            ImGui::ColorEdit4(
                                "##boxgradcol",
                                Vars::ESP::boxGradientColor,
                                ImGuiColorEditFlags_NoInputs |
                                ImGuiColorEditFlags_NoLabel |
                                ImGuiColorEditFlags_AlphaBar
                            );

                            Menu::SliderFloat(
                                "Box Thickness",
                                &Vars::ESP::boxThickness,
                                1.0f,
                                5.0f,
                                "%.1fpx"
                            );

                            ImGui::Text("Box Color");

                            ImGui::SameLine();
                            ImGui::SetCursorPosX(
                                ImGui::GetWindowWidth() -
                                ImGui::GetStyle().WindowPadding.x -
                                24.0f
                            );

                            ImGui::ColorEdit4(
                                "##boxcol",
                                Vars::ESP::boxColor,
                                ImGuiColorEditFlags_NoInputs |
                                ImGuiColorEditFlags_NoLabel |
                                ImGuiColorEditFlags_AlphaBar
                            );
                        }

                        Menu::CheckBox("Head Dot", &Vars::ESP::headDot);

                        ImGui::SameLine();
                        ImGui::SetCursorPosX(
                            ImGui::GetWindowWidth() -
                            ImGui::GetStyle().WindowPadding.x -
                            24.0f
                        );

                        ImGui::ColorEdit4(
                            "##headdotcol",
                            Vars::ESP::headDotColor,
                            ImGuiColorEditFlags_NoInputs |
                            ImGuiColorEditFlags_NoLabel |
                            ImGuiColorEditFlags_AlphaBar
                        );

                        Menu::CheckBox(
                            "View Angles",
                            &Vars::ESP::viewAngle
                        );

                        std::vector<const char*> nameModes =
                        {
                            "Off",
                            "Username",
                            "Display Name"
                        };

                        Menu::Combo(
                            "Name Mode",
                            &Vars::ESP::nameMode,
                            nameModes
                        );

                        if (Vars::ESP::nameMode != 0)
                        {
                            ImGui::Text("Name Color");

                            ImGui::SameLine();
                            ImGui::SetCursorPosX(
                                ImGui::GetWindowWidth() -
                                ImGui::GetStyle().WindowPadding.x -
                                24.0f
                            );

                            ImGui::ColorEdit4(
                                "##namecol",
                                Vars::ESP::nameColor,
                                ImGuiColorEditFlags_NoInputs |
                                ImGuiColorEditFlags_NoLabel |
                                ImGuiColorEditFlags_AlphaBar
                            );
                        }

                        Menu::CheckBox(
                            "Draw Distance",
                            &Vars::ESP::distance
                        );

                        ImGui::SameLine();
                        ImGui::SetCursorPosX(
                            ImGui::GetWindowWidth() -
                            ImGui::GetStyle().WindowPadding.x -
                            24.0f
                        );

                        ImGui::ColorEdit4(
                            "##distcol",
                            Vars::ESP::distanceColor,
                            ImGuiColorEditFlags_NoInputs |
                            ImGuiColorEditFlags_NoLabel |
                            ImGuiColorEditFlags_AlphaBar
                        );

                        Menu::CheckBox(
                            "Tool Name",
                            &Vars::ESP::toolName
                        );

                        ImGui::SameLine();
                        ImGui::SetCursorPosX(
                            ImGui::GetWindowWidth() -
                            ImGui::GetStyle().WindowPadding.x -
                            24.0f
                        );

                        ImGui::ColorEdit4(
                            "##ToolNameColor",
                            Vars::ESP::toolNameColor,
                            ImGuiColorEditFlags_NoInputs |
                            ImGuiColorEditFlags_NoLabel |
                            ImGuiColorEditFlags_AlphaBar
                        );

                        Menu::CheckBox(
                            "Gradient Text",
                            &Vars::ESP::textGradient
                        );

                        ImGui::SameLine();
                        ImGui::SetCursorPosX(
                            ImGui::GetWindowWidth() -
                            ImGui::GetStyle().WindowPadding.x -
                            24.0f
                        );

                        ImGui::ColorEdit4(
                            "##textgradcol",
                            Vars::ESP::textGradientColor,
                            ImGuiColorEditFlags_NoInputs |
                            ImGuiColorEditFlags_NoLabel |
                            ImGuiColorEditFlags_AlphaBar
                        );

                        Menu::CheckBox(
                            "Draw Health Bar",
                            &Vars::ESP::healthBar
                        );

                        Menu::CheckBox(
                            "Draw Health Text",
                            &Vars::ESP::healthText
                        );

                        Menu::CheckBox(
                            "Offscreen Indicators",
                            &Vars::ESP::offscreen
                        );
                    }

                    // ============================================================
// HITBOX TAB
// ============================================================

else if (visualsSubTab == 1)
{
    Menu::CheckBox(
        "Enable Hitbox",
        &Vars::Hitbox::enabled
    );

    ImGui::Spacing();

    std::vector<const char*> hitboxTargets =
    {
        "HumanoidRootPart",
        "Head"
    };

    Menu::Combo(
        "Target Part",
        &Vars::Hitbox::targetPart,
        hitboxTargets
    );

    ImGui::Spacing();

    Menu::SliderFloat(
        "Hitbox Width",
        &Vars::Hitbox::sizeX,
        1.0f,
        20.0f,
        "%.1f"
    );

    Menu::SliderFloat(
        "Hitbox Height",
        &Vars::Hitbox::sizeY,
        1.0f,
        20.0f,
        "%.1f"
    );

    Menu::SliderFloat(
        "Hitbox Depth",
        &Vars::Hitbox::sizeZ,
        1.0f,
        20.0f,
        "%.1f"
    );

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    Menu::CheckBox(
        "Hitbox Visual",
        &Vars::Hitbox::visual
    );

    ImGui::SameLine();
    ImGui::SetCursorPosX(
        ImGui::GetWindowWidth() -
        ImGui::GetStyle().WindowPadding.x -
        24.0f
    );

    ImGui::ColorEdit4(
        "##HitboxVisualColor",
        Vars::Hitbox::visualColor,
        ImGuiColorEditFlags_NoInputs |
        ImGuiColorEditFlags_NoLabel |
        ImGuiColorEditFlags_AlphaBar
    );

    if (Vars::Hitbox::visual)
    {
        Menu::SliderFloat(
            "Visual Thickness",
            &Vars::Hitbox::visualThickness,
            1.0f,
            5.0f,
            "%.1fpx"
        );
    }

    ImGui::Spacing();

    Menu::CheckBox(
        "Team Check##Hitbox",
        &Vars::Hitbox::teamCheck
    );

    Menu::CheckBox(
        "Dead Check##Hitbox",
        &Vars::Hitbox::deadCheck
    );
}

                    // ============================================================
                    // TEAM WHITELIST TAB
                    // ============================================================

                    else if (visualsSubTab == 2)
                    {
                        Menu::CheckBox(
                            "Enable Team Whitelist",
                            &Vars::ESP::teamWhitelistEnabled
                        );

                        ImGui::Spacing();

                        ImGui::TextDisabled(
                            "Whitelisted teams are ignored by ESP"
                        );

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        auto snapshot =
                            PlayerCache::GetSnapshot();

                        std::vector<uintptr_t> shownTeams;

                        bool foundTeam = false;

                        for (auto& p : snapshot)
                        {
                            if (
                                p.teamAddr == 0 ||
                                p.teamName.empty()
                                )
                                continue;

                            if (std::find(
                                shownTeams.begin(),
                                shownTeams.end(),
                                p.teamAddr
                            ) != shownTeams.end())
                                continue;

                            shownTeams.push_back(p.teamAddr);
                            foundTeam = true;

                            bool whitelisted =
                                std::find(
                                    Vars::ESP::whitelistedTeams.begin(),
                                    Vars::ESP::whitelistedTeams.end(),
                                    p.teamAddr
                                )
                                != Vars::ESP::whitelistedTeams.end();

                            ImGui::PushID(
                                reinterpret_cast<void*>(
                                    p.teamAddr
                                    )
                            );

                            if (Menu::CheckBox(
                                p.teamName.c_str(),
                                &whitelisted
                            ))
                            {
                                if (whitelisted)
                                {
                                    if (std::find(
                                        Vars::ESP::whitelistedTeams.begin(),
                                        Vars::ESP::whitelistedTeams.end(),
                                        p.teamAddr
                                    )
                                        == Vars::ESP::whitelistedTeams.end())
                                    {
                                        Vars::ESP::whitelistedTeams.push_back(
                                            p.teamAddr
                                        );
                                    }
                                }
                                else
                                {
                                    Vars::ESP::whitelistedTeams.erase(
                                        std::remove(
                                            Vars::ESP::whitelistedTeams.begin(),
                                            Vars::ESP::whitelistedTeams.end(),
                                            p.teamAddr
                                        ),
                                        Vars::ESP::whitelistedTeams.end()
                                    );
                                }
                            }

                            ImGui::PopID();
                        }

                        if (!foundTeam)
                        {
                            ImGui::TextDisabled(
                                "No teams found in the current game."
                            );
                        }
                    }

                    Menu::EndChild();
                }

                ImGui::SameLine(0.0f, Spacing);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY());
                if (Menu::BeginChild("Extra Indicators", ImVec2(RightWidth - SidePad, AvailSize.y - SidePad * 2.0f)))
                {
                    Menu::CheckBox(
                        "Snaplines (Tracers)",
                        &Vars::ESP::tracers
                    );

                    ImGui::SameLine();
                    ImGui::SetCursorPosX(
                        ImGui::GetWindowWidth() -
                        ImGui::GetStyle().WindowPadding.x -
                        24.0f
                    );

                    ImGui::ColorEdit4(
                        "##tracercol",
                        Vars::ESP::tracerColor,
                        ImGuiColorEditFlags_NoInputs |
                        ImGuiColorEditFlags_NoLabel |
                        ImGuiColorEditFlags_AlphaBar
                    );

                    if (Vars::ESP::tracers)
                    {
                        std::vector<const char*> tracerOrigins =
                        {
                            "Top",
                            "Center",
                            "Bottom",
                            "Mouse"
                        };

                        Menu::Combo(
                            "Tracer Origin",
                            &Vars::ESP::tracerOrigin,
                            tracerOrigins
                        );
                    }

                    Menu::CheckBox(
                        "Skeleton",
                        &Vars::ESP::skeleton
                    );

                    ImGui::SameLine();
                    ImGui::SetCursorPosX(
                        ImGui::GetWindowWidth() -
                        ImGui::GetStyle().WindowPadding.x -
                        24.0f
                    );

                    ImGui::ColorEdit4(
                        "##skelcol",
                        Vars::ESP::skeletonColor,
                        ImGuiColorEditFlags_NoInputs |
                        ImGuiColorEditFlags_NoLabel |
                        ImGuiColorEditFlags_AlphaBar
                    );

                    if (Vars::ESP::skeleton)
                    {
                        Menu::SliderFloat(
                            "Skel Thickness",
                            &Vars::ESP::skeletonThickness,
                            1.0f,
                            5.0f,
                            "%.1fpx"
                        );
                    }

                    Menu::CheckBox(
                        "Body Outline",
                        &Vars::ESP::bodyOutline
                    );

                    ImGui::SameLine();
                    ImGui::SetCursorPosX(
                        ImGui::GetWindowWidth() -
                        ImGui::GetStyle().WindowPadding.x -
                        24.0f
                    );

                    ImGui::ColorEdit4(
                        "##bodyoutlinecol",
                        Vars::ESP::bodyOutlineColor,
                        ImGuiColorEditFlags_NoInputs |
                        ImGuiColorEditFlags_NoLabel |
                        ImGuiColorEditFlags_AlphaBar
                    );

                    if (Vars::ESP::bodyOutline)
                    {
                        Menu::SliderFloat(
                            "Outline Thickness",
                            &Vars::ESP::bodyOutlineThickness,
                            1.0f,
                            5.0f,
                            "%.1fpx"
                        );
                    }

                    Menu::CheckBox(
                        "Filled Box",
                        &Vars::ESP::filledBox
                    );

                    ImGui::SameLine();
                    ImGui::SetCursorPosX(
                        ImGui::GetWindowWidth() -
                        ImGui::GetStyle().WindowPadding.x -
                        24.0f
                    );

                    ImGui::ColorEdit4(
                        "##fillcol",
                        Vars::ESP::filledColor,
                        ImGuiColorEditFlags_NoInputs |
                        ImGuiColorEditFlags_NoLabel |
                        ImGuiColorEditFlags_AlphaBar
                    );

                    Menu::CheckBox(
                        "Custom Crosshair",
                        &Vars::ESP::crosshair
                    );

                    ImGui::SameLine();
                    ImGui::SetCursorPosX(
                        ImGui::GetWindowWidth() -
                        ImGui::GetStyle().WindowPadding.x -
                        24.0f
                    );

                    ImGui::ColorEdit4(
                        "##crosscol",
                        Vars::ESP::crosshairColor,
                        ImGuiColorEditFlags_NoInputs |
                        ImGuiColorEditFlags_NoLabel |
                        ImGuiColorEditFlags_AlphaBar
                    );

                    if (Vars::ESP::crosshair)
                    {
                        Menu::SliderFloat(
                            "Size",
                            &Vars::ESP::crosshairSize,
                            1.0f,
                            30.0f,
                            "%.0f"
                        );

                        Menu::SliderFloat(
                            "Thickness",
                            &Vars::ESP::crosshairThickness,
                            1.0f,
                            10.0f,
                            "%.0f"
                        );

                        Menu::SliderFloat(
                            "Gap",
                            &Vars::ESP::crosshairGap,
                            0.0f,
                            20.0f,
                            "%.0f"
                        );

                        Menu::CheckBox(
                            "Dot",
                            &Vars::ESP::crosshairDot
                        );
                    }
                    Menu::EndChild();
                }
            }
           // else if (Vars::selectedTab == 2) // World
           // {
               // ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SidePad);
                //ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SidePad);

                //float fullH   = AvailSize.y - SidePad * 2.0f;

               // if (Menu::BeginChild("Skybox", ImVec2(LeftWidth - SidePad, fullH)))
               // {
                 //   Menu::CheckBox("Enable Skybox", &Vars::World::skybox);
                 //   if (Vars::World::skybox) {
                    //    std::vector<const char*> skyboxes = {
                         //   "Purple Galaxy",   // 0
                         //   "Evening Glow",    // 1 (Swapped with Minecraft)
                         //   "Sunny Day",       // 2
                        //    "Minecraft",       // 3 (Swapped with Evening Glow)
                        //    "Overcast",        // 4
                       //     "Anime Sky",       // 5
                       //     "Classic Roblox",  // 6
                       //     "Tropical Sunset", // 7
                       //     "Custom HDRI",     // 8
                       //     "Deep Galaxy",     // 9
                       //     "Cloudy Legacy",   // 10
                     //   };
                    //    Menu::Combo("Skybox Theme", &Vars::World::skyboxType, skyboxes);
                    //    Menu::CheckBox("Rotate Skybox", &Vars::World::rotateSkybox);
                    //    if (Vars::World::rotateSkybox) {
                    //        Menu::SliderFloat("Rotation Speed", &Vars::World::rotateSpeed, 0.1f, 5.0f, "%.1f");
                    //    }
                   // }
                   // Menu::EndChild();
              //  }

              //  ImGui::SameLine(0.0f, Spacing);
             //   float startY = ImGui::GetCursorPosY();
             //   ImGui::SetCursorPosY(startY + SidePad);
                
               // if (Menu::BeginChild("World Lighting", ImVec2(RightWidth - SidePad, fullH)))
               // {
                //    Menu::CheckBox("Override Ambience", &Vars::World::ambience);
                 //   if (Vars::World::ambience) {
                 //       ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                  //      ImGui::ColorEdit3("##ambcol", Vars::World::ambienceColor, ImGuiColorEditFlags_NoLabel);
                  //  }
//
                  //  Menu::CheckBox("Override Fog", &Vars::World::fog);
                   // if (Vars::World::fog) {
                   //     ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                  //      ImGui::ColorEdit3("##fogcol", Vars::World::fogColor, ImGuiColorEditFlags_NoLabel);
                  //      Menu::SliderFloat("Fog Distance", &Vars::World::fogDistance, 0.0f, 5000.0f, "%.0f");
                  //  }
//
                 //   Menu::CheckBox("Override Brightness", &Vars::World::brightness);
                  //  if (Vars::World::brightness) {
                  //      Menu::SliderFloat("Brightness", &Vars::World::brightnessValue, 0.0f, 10.0f, "%.1f");
                  //  }
//
                  //  Menu::CheckBox("Override Exposure", &Vars::World::exposure);
                  //  if (Vars::World::exposure) {
                  //      Menu::SliderFloat("Exposure", &Vars::World::exposureValue, -5.0f, 5.0f, "%.1f");
                  //  }
//
                 //   Menu::CheckBox("Override FOV", &Vars::World::fov);
                 //   if (Vars::World::fov) {
                //        Menu::SliderFloat("Field of View", &Vars::World::fovValue, 10.0f, 120.0f, "%.0f");
                //    }
//
                 //   Menu::EndChild();
                //}
            //}
            else if (Vars::selectedTab == 3) // Player
            {
                float startY = ImGui::GetCursorPosY(); // Top of the group 
                
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SidePad);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SidePad);

                if (Menu::BeginChild("Movement", ImVec2(LeftWidth - SidePad, HalfHeight)))
                {
                    Menu::CheckBox("Enable WalkSpeed", &Vars::Local::speedEnabled);
                    Menu::SliderFloat("WalkSpeed Value", &Vars::Local::walkSpeed, 16.0f, 300.0f, "%.0f");
                    
                    Menu::CheckBox("Enable JumpPower", &Vars::Local::jumpEnabled);
                    Menu::SliderFloat("JumpPower Value", &Vars::Local::jumpPower, 50.0f, 300.0f, "%.0f");

                    Menu::CheckBox("Infinite Jump", &Vars::Local::infiniteJumpEnabled);
                    Menu::EndChild();
                }

                float BottomBoxY = HalfHeight - 8;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SidePad);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SidePad);
                if (Menu::BeginChild("Desync Settings", ImVec2(LeftWidth - SidePad, BottomBoxY)))
                {
                    Menu::CheckBox("Enable Desync", &Vars::Local::desyncEnabled);
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - Menu::GetColorPickerWidth() + Style.WindowPadding.x + 12.0f);
                    Menu::KeyBind("Desync Keybind", &Vars::Local::desyncKey);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Spoofs network replication and physics state.");
                    
                    std::vector<const char*> desyncTypes = { "Server-sided", "Client-sided" };
                    Menu::Combo("Desync Type", &Vars::Local::desyncType, desyncTypes);

                    if (Vars::Local::desyncType == 0) {
                        std::vector<const char*> serverModes = { "Fling (9e9f)", "Position Jitter" };
                        Menu::Combo("Server Mode", &Vars::Local::serverMode, serverModes);
                    } else {
                        std::vector<const char*> clientModes = { "Angular NaN (Safe)", "Linear NaN (Lag)", "Process Suspend" };
                        Menu::Combo("Client Mode", &Vars::Local::clientMode, clientModes);
                    }
                    Menu::EndChild();
                }

                ImGui::SameLine(0.0f, Spacing);
                ImGui::SetCursorPosY(startY + SidePad);
                if (Menu::BeginChild("Flight & Freecam", ImVec2(RightWidth - SidePad, AvailSize.y - SidePad * 2.0f)))
                {
                    // Flight
                    Menu::CheckBox("Enable Flight", &Vars::Local::flightEnabled);

                    ImGui::SameLine(
                        ImGui::GetContentRegionAvail().x -
                        Menu::GetColorPickerWidth() +
                        Style.WindowPadding.x + 12.0f
                    );

                    Menu::KeyBind("Flight Keybind", &Vars::Local::flightKey);

                    Menu::SliderFloat(
                        "Flight Speed",
                        &Vars::Local::flightSpeed,
                        10.0f,
                        200.0f,
                        "%.0f"
                    );

                    ImGui::Separator();

                    Menu::CheckBox("Enable Freecam", &Vars::Local::freecamEnabled);
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - Menu::GetColorPickerWidth() + Style.WindowPadding.x + 12.0f);
                    Menu::KeyBind("Freecam Keybind", &Vars::Local::freecamKey);

                    Menu::SliderFloat(
                        "Freecam Speed",
                        &Vars::Local::freecamSpeed,
                        5.0f,
                        500.0f,
                        "%.0f"
                    );

                    ImGui::Separator();

                    // Headless
                    Menu::CheckBox(
                        "Headless",
                        &Vars::Local::headlessEnabled
                    );

                    ImGui::Separator();

                    Menu::EndChild();
                }
            }
            else if (Vars::selectedTab == 4) // Rage
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SidePad);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SidePad);

                if (Menu::BeginChild("Anti-Aim", ImVec2(LeftWidth - SidePad, HalfHeight)))
                {
                    Menu::CheckBox("Enable Anti-Aim", &Vars::Rage::antiAim);
                    Menu::EndChild();
                }
            }
            else if (Vars::selectedTab == 2) // Misc (Player List)
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SidePad);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SidePad);

                if (Menu::BeginChild("Player List Box", ImVec2(AvailSize.x - SidePad * 2.0f, AvailSize.y - SidePad * 2.0f)))
                {
                    ImGui::Spacing();
                    
                    // Title "Player List" in Red (using accent color style)
                    ImGui::PushStyleColor(
                        ImGuiCol_Text,
                        ImGui::ColorConvertU32ToFloat4(Menu::Accent)
                    );
                    ImGui::SetWindowFontScale(1.3f);
                    ImGui::Text("Player List");
                    ImGui::SetWindowFontScale(1.0f);
                    ImGui::PopStyleColor();

                    ImGui::Spacing();

                    // ============================================================
                    // SEARCH BAR
                    // ============================================================

                    static char playerSearch[128] = "";

                    ImGui::SetNextItemWidth(-1.0f);

                    ImGui::PushStyleVar(
                        ImGuiStyleVar_FrameRounding,
                        4.0f
                    );

                    ImGui::PushStyleColor(
                        ImGuiCol_FrameBg,
                        ImVec4(0.06f, 0.06f, 0.065f, 1.0f)
                    );

                    ImGui::PushStyleColor(
                        ImGuiCol_FrameBgHovered,
                        ImVec4(0.08f, 0.08f, 0.085f, 1.0f)
                    );

                    ImGui::PushStyleColor(
                        ImGuiCol_FrameBgActive,
                        ImVec4(0.09f, 0.09f, 0.095f, 1.0f)
                    );

                    bool searchChanged =
                        ImGui::InputTextWithHint(
                            "##PlayerSearch",
                            "Search player...",
                            playerSearch,
                            IM_ARRAYSIZE(playerSearch)
                        );

                    if (ImGui::IsItemClicked())
                    {
                        SetForegroundWindow(windowHandle);
                        SetFocus(windowHandle);
                    }

                    ImGui::PopStyleColor(3);
                    ImGui::PopStyleVar();

                    ImGui::Spacing();

                    // ============================================================
// TABLE HEADER
// ============================================================

                    float availWidth =
                        ImGui::GetContentRegionAvail().x;

                    ImGui::Text("Players");

                    ImGui::SameLine();

                    ImGui::SetCursorPosX(
                        ImGui::GetWindowWidth() -
                        210.0f
                    );

                    if (ImGui::Button(
                        Vars::Misc::flingAllActive
                        ? "Flinging..."
                        : "Fling All",
                        ImVec2(90.0f, 25.0f)
                    ))
                    {
                        StartFlingAll();
                    }

                    ImGui::SameLine();

                    ImGui::TextDisabled("Actions");

                    ImGui::Separator();
                    ImGui::Spacing();

                    // Scrollable list
                    ImGui::BeginChild("##player_scroll_list", ImVec2(0, 0), false, ImGuiWindowFlags_NoBackground);

                    auto snapshot = PlayerCache::GetSnapshot();
                    ImDrawList* drawList = ImGui::GetWindowDrawList();

                    for (auto& plr : snapshot)
                    {
                        if (!plr.isValid)
                            continue;

                        // ========================================================
                        // SEARCH FILTER
                        // ========================================================

                        if (playerSearch[0] != '\0')
                        {
                            std::string query = playerSearch;
                            std::string username = plr.name;
                            std::string displayName = plr.displayName;
                            std::string teamName = plr.teamName;

                            // Case-insensitive search
                            std::transform(
                                query.begin(),
                                query.end(),
                                query.begin(),
                                [](unsigned char c)
                                {
                                    return static_cast<char>(std::tolower(c));
                                }
                            );

                            std::transform(
                                username.begin(),
                                username.end(),
                                username.begin(),
                                [](unsigned char c)
                                {
                                    return static_cast<char>(std::tolower(c));
                                }
                            );

                            std::transform(
                                displayName.begin(),
                                displayName.end(),
                                displayName.begin(),
                                [](unsigned char c)
                                {
                                    return static_cast<char>(std::tolower(c));
                                }
                            );

                            std::transform(
                                teamName.begin(),
                                teamName.end(),
                                teamName.begin(),
                                [](unsigned char c)
                                {
                                    return static_cast<char>(std::tolower(c));
                                }
                            );

                            bool matches =
                                username.find(query) != std::string::npos ||
                                displayName.find(query) != std::string::npos ||
                                teamName.find(query) != std::string::npos;

                            if (!matches)
                                continue;
                        }

                        float startY = ImGui::GetCursorPosY();

                        // Minimalistic vector profile box placeholder (0% lag)
                        ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
                        drawList->AddRectFilled(cursorScreenPos, cursorScreenPos + ImVec2(40.0f, 40.0f), IM_COL32(24, 24, 24, 255), 4.0f);
                        drawList->AddRect(cursorScreenPos, cursorScreenPos + ImVec2(40.0f, 40.0f), IM_COL32(45, 45, 45, 255), 4.0f);
                        
                        // Simple local vector icon: head & shoulders silhouette
                        float centerX = cursorScreenPos.x + 20.0f;
                        float centerY = cursorScreenPos.y + 15.0f;
                        drawList->AddCircleFilled(ImVec2(centerX, centerY), 7.0f, IM_COL32(180, 180, 180, 255));
                        drawList->AddRectFilled(ImVec2(cursorScreenPos.x + 8.0f, cursorScreenPos.y + 27.0f), ImVec2(cursorScreenPos.x + 32.0f, cursorScreenPos.y + 40.0f), IM_COL32(120, 120, 120, 255), 3.0f);

                        ImGui::Dummy(ImVec2(40.0f, 40.0f));

                        // Names Group next to avatar box
                        ImGui::SameLine(55.0f);
                        ImGui::BeginGroup();
                        // Display name in red
                        ImGui::PushStyleColor(
                            ImGuiCol_Text,
                            ImGui::ColorConvertU32ToFloat4(Menu::Accent)
                        );
                        ImGui::Text("%s", plr.displayName.c_str());
                        ImGui::PopStyleColor();

                        // Username in dark gray
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.00f));
                        ImGui::Text("(@%s)", plr.name.c_str());
                        ImGui::PopStyleColor();

                        // Team name
                        if (!plr.teamName.empty())
                        {
                            ImGui::PushStyleColor(
                                ImGuiCol_Text,
                                ImVec4(0.65f, 0.65f, 0.65f, 1.00f)
                            );

                            ImGui::Text("%s", plr.teamName.c_str());

                            ImGui::PopStyleColor();
                        }

                        ImGui::EndGroup();

                        // ========================================================
// FLING TARGET STATUS
// ========================================================

                        bool isFlingTarget =
                            IsFlingTarget(plr.name);

                        ImGui::SameLine(
                            availWidth - 260.0f
                        );

                        ImGui::BeginGroup();

                        ImGui::TextDisabled(
                            "Fling Target"
                        );

                        if (isFlingTarget)
                        {
                            ImGui::PushStyleColor(
                                ImGuiCol_Text,
                                ImGui::ColorConvertU32ToFloat4(
                                    Menu::Accent
                                )
                            );

                            ImGui::TextUnformatted(
                                "Targeted"
                            );

                            ImGui::PopStyleColor();
                        }
                        else
                        {
                            ImGui::TextDisabled(
                                "None"
                            );
                        }

                        ImGui::EndGroup();

                        // ========================================================
                        // ACTIONS BUTTON
                        // ========================================================

                        ImGui::SameLine(
                            availWidth - 105.0f
                        );

                        ImGui::PushStyleVar(
                            ImGuiStyleVar_FrameBorderSize,
                            1.0f
                        );

                        ImGui::PushStyleVar(
                            ImGuiStyleVar_FrameRounding,
                            3.0f
                        );

                        ImGui::PushStyleColor(
                            ImGuiCol_Border,
                            ImGui::ColorConvertU32ToFloat4(
                                Menu::Accent
                            )
                        );

                        ImGui::PushStyleColor(
                            ImGuiCol_Button,
                            ImVec4(
                                0.08f,
                                0.08f,
                                0.09f,
                                1.0f
                            )
                        );

                        if (ImGui::Button(
                            ("Actions##" + plr.name).c_str(),
                            ImVec2(95.0f, 27.0f)
                        ))
                        {
                            Vars::Misc::actionTargetName =
                                plr.name;

                            Vars::Misc::playerActionsOpen =
                                true;
                        }

                        ImGui::PopStyleColor(2);
                        ImGui::PopStyleVar(2);

                        // Adjust row cursor position and separator
                        ImGui::SetCursorPosY(startY + 62.0f);
                        ImGui::Separator();
                        ImGui::Spacing();
                    }

                    ImGui::EndChild();
                    Menu::EndChild();
                }
            }
            else if (Vars::selectedTab == 5) // Settings
            {
                float fullH   = AvailSize.y - SidePad * 2.0f;
                float MiscHalf   = fullH * 0.5f - SidePad * 0.5f;
                float ConfigBoxH = fullH - MiscHalf - SidePad;

                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SidePad);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SidePad);

                if (Menu::BeginChild("Settings", ImVec2(LeftWidth - SidePad, 190.0f)))
                {
                    Menu::CheckBox("Stream Proof", &Vars::Misc::streamProof);
                    Menu::CheckBox("Fast Launch", &Vars::Misc::fastLaunch);
                    Menu::CheckBox("Auto-rescan on Game Join", &Vars::Misc::autoRescan);
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    float btnW = (ImGui::GetContentRegionAvail().x - Style.ItemSpacing.x);
                    if (ImGui::Button("Rescan Pointers", ImVec2(btnW, 30))) {
                        Vars::Misc::rescan = true;
                    }

                    Menu::EndChild();
                }

                // ==========================================================
                // MENU OPTIMIZATION
                // ==========================================================

                    float settingsRightX = ContentMin.x + LeftWidth + Spacing;
                    float settingsTopY = ContentMin.y + SidePad;

                    ImGui::SetCursorScreenPos(
                        ImVec2(settingsRightX, settingsTopY)
                    );

                    if (Menu::BeginChild(
                        "Menu Optimization",
                        ImVec2(RightWidth - SidePad, AvailSize.y - SidePad * 2.0f)
                    ))
                    {
                        Menu::CheckBox(
                            "Dark Background",
                            &Vars::Settings::darkBackground
                        );

                        Menu::CheckBox(
                            "Show Status Bar",
                            &Vars::Settings::showWatermark
                        );

                        Menu::CheckBox(
                            "Animated Particles",
                            &Vars::Settings::menuParticles
                        );

                        Menu::CheckBox(
                            "Menu Glow",
                            &Vars::Settings::menuGlow
                        );

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        ImGui::Text("Menu Theme");

                        Menu::ColorEdit4(
                            "##MenuTheme",
                            Vars::Settings::themeColor
                        );

                        Menu::EndChild();
                    }

                    // CONFIG POSITION
                    ImGui::SetCursorScreenPos(
                        ImVec2(
                            ContentMin.x + SidePad,
                            ContentMin.y + 190.0f + SidePad * 2.0f
                        )
                    );

                    static char cfgNameBuf[64] = "default";
                static std::vector<std::string> cfgList;
                static bool cfgListDirty = true;

                if (Menu::BeginChild(
                    "Config",
                    ImVec2(LeftWidth - SidePad, AvailSize.y - 190.0f - SidePad * 3.0f)
                ))
                {
                    ImGui::Text("Config Name");
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::InputText("##cfgname", cfgNameBuf, sizeof(cfgNameBuf));

                    float btnW = (ImGui::GetContentRegionAvail().x - Style.ItemSpacing.x) * 0.5f;
                    if (ImGui::Button("Save##cfg", ImVec2(btnW, 0))) {
                        if (Config::Save(cfgNameBuf)) {
                            NotificationService::Push("Config Saved successfully!", 2.5f);
                        } else {
                            NotificationService::Push("Failed to save config.", 2.5f);
                        }
                        cfgListDirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Load##cfg", ImVec2(btnW, 0))) {
                        if (Config::Load(cfgNameBuf)) {
                            NotificationService::Push("Config Loaded successfully!", 2.5f);
                        } else {
                            NotificationService::Push("Failed to load config.", 2.5f);
                        }
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    if (cfgListDirty) {
                        cfgList = Config::ListConfigs();
                        cfgListDirty = false;
                    }

                    ImGui::Text("Saved Configs:");
                    for (auto& name : cfgList) {
                        bool sel = (name == cfgNameBuf);
                        if (ImGui::Selectable(name.c_str(), sel)) {
                            strncpy_s(cfgNameBuf, name.c_str(), sizeof(cfgNameBuf) - 1);
                        }
                    }

                    Menu::EndChild();
                }
            }

            ImGui::EndGroup();
            ImGui::PopClipRect();
        }

        // Save the actual CURRENT menu position.
// This changes whenever you drag the main menu.
        ImVec2 currentMenuPos = ImGui::GetWindowPos();
        ImVec2 currentMenuSize = ImGui::GetWindowSize();

        // ============================================================
// MENU GLOW
// ============================================================

        if (Vars::Settings::menuGlow)
        {
            ImDrawList* glowDraw =
                ImGui::GetBackgroundDrawList();

            ImVec2 glowMin =
                ImGui::GetWindowPos();

            ImVec2 glowSize =
                ImGui::GetWindowSize();

            ImVec2 glowMax(
                glowMin.x + glowSize.x,
                glowMin.y + glowSize.y
            );

            ImVec4 theme(
                Vars::Settings::themeColor[0],
                Vars::Settings::themeColor[1],
                Vars::Settings::themeColor[2],
                1.0f
            );

            // Multiple soft layers instead of one ugly thick border.
            for (int i = 8; i >= 1; --i)
            {
                float spread =
                    static_cast<float>(i) * 2.0f;

                float alpha =
                    (9.0f - static_cast<float>(i)) /
                    9.0f;

                alpha *= 0.10f;

                ImU32 glowColor =
                    ImGui::ColorConvertFloat4ToU32(
                        ImVec4(
                            theme.x,
                            theme.y,
                            theme.z,
                            alpha
                        )
                    );

                glowDraw->AddRect(
                    ImVec2(
                        glowMin.x - spread,
                        glowMin.y - spread
                    ),
                    ImVec2(
                        glowMax.x + spread,
                        glowMax.y + spread
                    ),
                    glowColor,
                    10.0f + spread * 0.25f,
                    0,
                    2.5f
                );
            }

            // Crisp inner accent.
            glowDraw->AddRect(
                glowMin,
                glowMax,
                ImGui::ColorConvertFloat4ToU32(
                    ImVec4(
                        theme.x,
                        theme.y,
                        theme.z,
                        0.45f
                    )
                ),
                8.0f,
                0,
                1.0f
            );
        }

        ImGui::End();

        RenderPlayerActionsWindow(
            currentMenuPos,
            currentMenuSize
        );

        UpdateFling();
        UpdateFlingAll();

        RenderFlingTargetsWindow();
        RenderEnemyListWindow();

        RenderESPPreview(
            menuAlpha,
            currentMenuPos,
            currentMenuSize
        );

        // Draw actual in-game Aimbot FOV
        RenderAimbotFOV();

        ImGui::PopStyleVar(); // menu fade alpha
}

    void EndFrame() {
        ImGui::Render();

        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        d3dContext->OMSetRenderTargets(1, &renderTarget, nullptr);
        d3dContext->ClearRenderTargetView(renderTarget, clearColor);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        swapChain->Present(0, 0);
    }

    void Cleanup() {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        CleanupD3D11();

        if (windowHandle) {
            DestroyWindow(windowHandle);
            windowHandle = nullptr;
        }

        UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
    }

    HWND GetWindowHandle() const { return windowHandle; }
};
