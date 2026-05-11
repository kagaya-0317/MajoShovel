#include "debug/DebugConsole.hpp"

#include "debug/DebugPanel.hpp"

#include <algorithm>
#include <condition_variable>
#include <cctype>
#include <chrono>
#include <deque>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <richedit.h>
#include <commctrl.h>
#endif

namespace majo {

namespace {

constexpr std::size_t MaxLogLines = 2000;
constexpr std::size_t MaxHistory = 80;

std::string trimAscii(std::string text)
{
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    text.erase(text.begin(), std::find_if_not(text.begin(), text.end(), isSpace));
    text.erase(std::find_if_not(text.rbegin(), text.rend(), isSpace).base(), text.end());
    return text;
}

#ifdef _WIN32

constexpr UINT WM_DEBUG_APPEND_LOG = WM_APP + 1;
constexpr UINT WM_DEBUG_TOGGLE = WM_APP + 2;
constexpr UINT WM_DEBUG_SHUTDOWN = WM_APP + 3;
constexpr UINT WM_DEBUG_SCROLL_TO_BOTTOM = WM_APP + 4;

constexpr int ControlLog = 1001;
constexpr int ControlCommand = 1002;
constexpr int ControlRun = 1003;
constexpr int ControlClear = 1004;
constexpr int ControlCopy = 1005;
constexpr int ControlPause = 1006;
constexpr int ControlAutoScroll = 1007;
constexpr int ControlDebugBase = 2000;

std::wstring utf8ToWide(std::string_view text)
{
    if (text.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }

    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

std::string wideToUtf8(std::wstring_view text)
{
    if (text.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

const wchar_t* levelName(LogLevel level)
{
    switch (level) {
    case LogLevel::Info: return L"info";
    case LogLevel::Warning: return L"warn";
    case LogLevel::Error: return L"error";
    case LogLevel::Command: return L"cmd";
    }
    return L"log";
}

COLORREF levelColor(LogLevel level)
{
    switch (level) {
    case LogLevel::Info: return RGB(42, 42, 42);
    case LogLevel::Warning: return RGB(154, 94, 0);
    case LogLevel::Error: return RGB(184, 45, 34);
    case LogLevel::Command: return RGB(0, 92, 170);
    }
    return RGB(42, 42, 42);
}

std::wstring timestampText()
{
    SYSTEMTIME now{};
    GetLocalTime(&now);

    wchar_t buffer[16]{};
    swprintf_s(buffer, L"%02u:%02u:%02u", now.wHour, now.wMinute, now.wSecond);
    return buffer;
}

#endif

}

struct DebugConsole::Impl {
    struct LogEntry {
        LogLevel level = LogLevel::Info;
        std::string message;
    };

#ifdef _WIN32
    struct DebugControlUi {
        HWND hwnd = nullptr;
        int controlId = 0;
        std::string command;
    };

    struct DebugGroupUi {
        HWND box = nullptr;
        std::vector<DebugControlUi> controls;
    };

    struct DebugTabUi {
        std::vector<DebugGroupUi> groups;
    };
#endif

    void initialize()
    {
#ifdef _WIN32
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (running) {
                return;
            }
            running = true;
            ready = false;
        }

        uiThread = std::thread([this] {
            runUiThread();
        });

        std::unique_lock<std::mutex> lock(mutex);
        readyCv.wait(lock, [this] { return ready; });
#endif
    }

    void shutdown()
    {
#ifdef _WIN32
        HWND target = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!running) {
                return;
            }
            shuttingDown = true;
            target = hwnd;
        }

        if (target) {
            PostMessageW(target, WM_DEBUG_SHUTDOWN, 0, 0);
        }
        if (uiThread.joinable()) {
            uiThread.join();
        }

        std::lock_guard<std::mutex> lock(mutex);
        running = false;
        shuttingDown = false;
        hwnd = nullptr;
#endif
    }

    void toggleVisible()
    {
#ifdef _WIN32
        HWND target = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex);
            target = hwnd;
        }
        if (target) {
            PostMessageW(target, WM_DEBUG_TOGGLE, 0, 0);
        }
#endif
    }

    void appendLog(LogLevel level, std::string_view message)
    {
#ifdef _WIN32
        HWND target = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!running) {
                return;
            }
            pendingLogs.push_back({level, std::string(message)});
            target = hwnd;
        }
        if (target) {
            PostMessageW(target, WM_DEBUG_APPEND_LOG, 0, 0);
        }
#else
        (void)level;
        (void)message;
#endif
    }

    std::optional<std::string> pollCommand()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (commandQueue.empty()) {
            return std::nullopt;
        }

        std::string command = std::move(commandQueue.front());
        commandQueue.pop_front();
        return command;
    }

#ifdef _WIN32
    void runUiThread()
    {
        HINSTANCE instance = GetModuleHandleW(nullptr);
        richEditLibrary = LoadLibraryW(L"Msftedit.dll");
        INITCOMMONCONTROLSEX commonControls{};
        commonControls.dwSize = sizeof(commonControls);
        commonControls.dwICC = ICC_TAB_CLASSES | ICC_BAR_CLASSES | ICC_PROGRESS_CLASS;
        InitCommonControlsEx(&commonControls);
        const wchar_t* className = L"MajoShovelDebugConsoleWindow";

        WNDCLASSW wc{};
        wc.lpfnWndProc = &DebugConsole::Impl::windowProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = className;
        RegisterClassW(&wc);

        HWND created = CreateWindowExW(
            WS_EX_TOOLWINDOW,
            className,
            L"Majo Shovel Debug Console",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            588,
            620,
            nullptr,
            nullptr,
            instance,
            this);

        {
            std::lock_guard<std::mutex> lock(mutex);
            hwnd = created;
            ready = true;
        }
        readyCv.notify_all();

        if (!created) {
            return;
        }

        ShowWindow(created, SW_HIDE);
        UpdateWindow(created);

        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_F8) {
                toggleWindow();
                continue;
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_F5) {
                queueCommand("restart");
                continue;
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN &&
                (msg.hwnd == commandCombo || IsChild(commandCombo, msg.hwnd))) {
                submitCommand();
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        DestroyWindowHandles();
    }

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        Impl* impl = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            impl = static_cast<Impl*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl));
        }

        if (!impl) {
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }

        switch (message) {
        case WM_CREATE:
            impl->createControls(hwnd);
            return 0;
        case WM_SIZE:
            impl->layoutControls(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_COMMAND:
            impl->handleCommand(LOWORD(wParam), HIWORD(wParam));
            return 0;
        case WM_NOTIFY:
            impl->handleNotify(reinterpret_cast<NMHDR*>(lParam));
            return 0;
        case WM_DEBUG_APPEND_LOG:
            impl->drainPendingLogs();
            return 0;
        case WM_DEBUG_TOGGLE:
            impl->toggleWindow();
            return 0;
        case WM_DEBUG_SHUTDOWN:
            impl->shuttingDown = true;
            DestroyWindow(hwnd);
            return 0;
        case WM_DEBUG_SCROLL_TO_BOTTOM:
            impl->drainPendingLogs();
            impl->scrollLogToBottom();
            return 0;
        case WM_CLOSE:
            if (impl->shuttingDown) {
                DestroyWindow(hwnd);
            } else {
                ShowWindow(hwnd, SW_HIDE);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    void createControls(HWND parent)
    {
        logFont = CreateFontW(
            18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FIXED_PITCH | FF_MODERN, L"Consolas");
        uiFont = CreateFontW(
            16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, L"Yu Gothic UI");

        logEdit = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            richEditLibrary ? L"RICHEDIT50W" : L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 0, 0,
            parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ControlLog)),
            GetModuleHandleW(nullptr),
            nullptr);
        SendMessageW(logEdit, WM_SETFONT, reinterpret_cast<WPARAM>(logFont), TRUE);
        SendMessageW(logEdit, EM_SETLIMITTEXT, 2 * 1024 * 1024, 0);
        SendMessageW(logEdit, EM_SETBKGNDCOLOR, 0, RGB(248, 248, 248));

        commandCombo = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"COMBOBOX",
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWN | CBS_HASSTRINGS,
            0, 0, 0, 0,
            parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ControlCommand)),
            GetModuleHandleW(nullptr),
            nullptr);
        SendMessageW(commandCombo, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont), TRUE);

        createDebugPanelControls(parent);

        runButton = createButton(parent, L"Run", ControlRun);
        clearButton = createButton(parent, L"Clear", ControlClear);
        copyButton = createButton(parent, L"Copy", ControlCopy);
        pauseButton = createButton(parent, L"Pause log", ControlPause);

        autoScrollCheck = CreateWindowExW(
            0,
            L"BUTTON",
            L"Auto scroll",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, 0, 0,
            parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ControlAutoScroll)),
            GetModuleHandleW(nullptr),
            nullptr);
        SendMessageW(autoScrollCheck, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont), TRUE);
        SendMessageW(autoScrollCheck, BM_SETCHECK, BST_CHECKED, 0);

        appendUiLog({LogLevel::Info, "Debug console ready. F8 shows or hides this window."});
        appendUiLog({LogLevel::Info, "Built-in commands: help, clear, restart, quit"});
    }

    void createDebugPanelControls(HWND parent)
    {
        layoutDefinition = makeDefaultDebugConsoleLayout();

        tabControl = CreateWindowExW(
            0,
            WC_TABCONTROLW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0, 0, 0, 0,
            parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ControlDebugBase - 1)),
            GetModuleHandleW(nullptr),
            nullptr);
        SendMessageW(tabControl, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont), TRUE);

        for (std::size_t index = 0; index < layoutDefinition.tabs.size(); ++index) {
            TCITEMW item{};
            item.mask = TCIF_TEXT;
            std::wstring label = utf8ToWide(layoutDefinition.tabs[index].label);
            item.pszText = label.data();
            TabCtrl_InsertItem(tabControl, static_cast<int>(index), &item);
        }

        int nextControlId = ControlDebugBase;
        for (std::size_t tabIndex = 0; tabIndex < layoutDefinition.tabs.size(); ++tabIndex) {
            const DebugTabDefinition& tab = layoutDefinition.tabs[tabIndex];
            DebugTabUi tabUi;
            for (std::size_t groupIndex = 0; groupIndex < tab.groups.size(); ++groupIndex) {
                const DebugGroupDefinition& group = tab.groups[groupIndex];
                DebugGroupUi groupUi;
                groupUi.box = CreateWindowExW(
                    0,
                    L"BUTTON",
                    utf8ToWide(group.label).c_str(),
                    WS_CHILD | BS_GROUPBOX,
                    0, 0, 0, 0,
                    parent,
                    nullptr,
                    GetModuleHandleW(nullptr),
                    nullptr);
                SendMessageW(groupUi.box, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont), TRUE);

                for (const DebugControlDefinition& control : group.controls) {
                    if (control.kind != DebugControlKind::Button) {
                        continue;
                    }

                    const int controlId = nextControlId++;
                    HWND button = createButton(parent, utf8ToWide(control.label).c_str(), controlId);
                    ShowWindow(button, SW_HIDE);
                    groupUi.controls.push_back({button, controlId, control.command});
                    debugCommandByControlId[controlId] = control.command;
                }

                ShowWindow(groupUi.box, SW_HIDE);
                tabUi.groups.push_back(std::move(groupUi));
            }
            debugTabs.push_back(std::move(tabUi));
        }
    }

    HWND createButton(HWND parent, const wchar_t* text, int controlId)
    {
        HWND button = CreateWindowExW(
            0,
            L"BUTTON",
            text,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0,
            parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
            GetModuleHandleW(nullptr),
            nullptr);
        SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont), TRUE);
        return button;
    }

    void layoutControls(int width, int height)
    {
        const int margin = 10;
        const int rowHeight = 30;
        const int gap = 8;
        const int buttonWidth = 86;
        const int wideButtonWidth = 96;
        const int buttonsY = std::max(margin, height - margin - rowHeight);
        const int commandY = std::max(margin, buttonsY - gap - rowHeight);
        const int panelHeight = std::min(440, std::max(300, (height * 2) / 3));
        const int panelY = std::max(margin, commandY - gap - panelHeight);
        const int logHeight = std::max(80, panelY - margin - gap);
        const int comboWidth = std::max(180, width - margin * 2);

        MoveWindow(logEdit, margin, margin, std::max(100, width - margin * 2), logHeight, TRUE);
        MoveWindow(tabControl, margin, panelY, std::max(100, width - margin * 2), panelHeight, TRUE);
        layoutDebugPanel(margin, panelY, std::max(100, width - margin * 2), panelHeight);
        MoveWindow(commandCombo, margin, commandY, comboWidth, 220, TRUE);

        int x = margin;
        MoveWindow(runButton, x, buttonsY, buttonWidth, rowHeight, TRUE);
        x += buttonWidth + gap;
        MoveWindow(clearButton, x, buttonsY, buttonWidth, rowHeight, TRUE);
        x += buttonWidth + gap;
        MoveWindow(copyButton, x, buttonsY, buttonWidth, rowHeight, TRUE);
        x += buttonWidth + gap;
        MoveWindow(pauseButton, x, buttonsY, wideButtonWidth, rowHeight, TRUE);
        x += wideButtonWidth + gap;
        MoveWindow(autoScrollCheck, x, buttonsY, 130, rowHeight, TRUE);
    }

    void handleCommand(int controlId, int notification)
    {
        if (notification != BN_CLICKED && notification != CBN_SELENDOK) {
            return;
        }

        if (notification == BN_CLICKED) {
            auto it = debugCommandByControlId.find(controlId);
            if (it != debugCommandByControlId.end()) {
                queueCommand(it->second);
                return;
            }
        }

        switch (controlId) {
        case ControlRun:
            submitCommand();
            break;
        case ControlClear:
            clearLogs();
            break;
        case ControlCopy:
            copyLogsToClipboard();
            break;
        case ControlPause:
            paused = !paused;
            SetWindowTextW(pauseButton, paused ? L"Resume log" : L"Pause log");
            if (!paused) {
                rebuildLogText();
            }
            break;
        case ControlAutoScroll:
            autoScroll = SendMessageW(autoScrollCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            break;
        default:
            break;
        }
    }

    void handleNotify(const NMHDR* header)
    {
        if (!header || header->hwndFrom != tabControl || header->code != TCN_SELCHANGE) {
            return;
        }

        currentTabIndex = std::max(0, TabCtrl_GetCurSel(tabControl));
        RECT rect{};
        GetClientRect(hwnd, &rect);
        layoutControls(rect.right - rect.left, rect.bottom - rect.top);
    }

    void layoutDebugPanel(int x, int y, int width, int height)
    {
        if (!tabControl) {
            return;
        }

        RECT content{0, 0, width, height};
        TabCtrl_AdjustRect(tabControl, FALSE, &content);
        const int contentX = x + content.left + 8;
        const int contentY = y + content.top + 8;
        const int contentW = std::max(1, static_cast<int>(content.right - content.left - 16));
        const int contentH = std::max(1, static_cast<int>(content.bottom - content.top - 16));

        for (std::size_t tabIndex = 0; tabIndex < debugTabs.size(); ++tabIndex) {
            DebugTabUi& tab = debugTabs[tabIndex];
            const bool visible = static_cast<int>(tabIndex) == currentTabIndex;
            layoutDebugTab(tab, visible, contentX, contentY, contentW, contentH);
        }
    }

    void layoutDebugTab(DebugTabUi& tab, bool visible, int x, int y, int width, int height)
    {
        const int groupGap = 10;
        const int groupCount = static_cast<int>(tab.groups.size());
        const int groupWidth = groupCount > 0
            ? std::max(120, (width - groupGap * (groupCount - 1)) / groupCount)
            : width;

        for (int groupIndex = 0; groupIndex < groupCount; ++groupIndex) {
            DebugGroupUi& group = tab.groups[static_cast<std::size_t>(groupIndex)];
            const int groupX = x + groupIndex * (groupWidth + groupGap);
            const int actualGroupWidth = groupIndex == groupCount - 1
                ? std::max(120, width - (groupX - x))
                : groupWidth;
            MoveWindow(group.box, groupX, y, actualGroupWidth, height, TRUE);
            ShowWindow(group.box, visible ? SW_SHOW : SW_HIDE);

            const int buttonX = groupX + 12;
            int buttonY = y + 28;
            const int buttonW = std::max(80, actualGroupWidth - 24);
            constexpr int ButtonH = 28;
            constexpr int ButtonGap = 8;
            for (DebugControlUi& control : group.controls) {
                MoveWindow(control.hwnd, buttonX, buttonY, buttonW, ButtonH, TRUE);
                ShowWindow(control.hwnd, visible ? SW_SHOW : SW_HIDE);
                buttonY += ButtonH + ButtonGap;
            }
        }
    }

    void submitCommand()
    {
        const int length = GetWindowTextLengthW(commandCombo);
        std::wstring wide(static_cast<std::size_t>(length) + 1, L'\0');
        if (length > 0) {
            GetWindowTextW(commandCombo, wide.data(), length + 1);
        }
        wide.resize(static_cast<std::size_t>(length));

        std::string command = trimAscii(wideToUtf8(wide));
        if (command.empty()) {
            return;
        }

        SetWindowTextW(commandCombo, L"");
        addHistory(command);

        const std::string lower = lowerAscii(command);
        if (lower == "clear") {
            clearLogs();
            return;
        }
        if (lower == "help") {
            appendUiLog({LogLevel::Command, "> " + command});
            appendUiLog({LogLevel::Info, "help: show this list"});
            appendUiLog({LogLevel::Info, "clear: clear console logs"});
            appendUiLog({LogLevel::Info, "restart: restart the current test-play run"});
            appendUiLog({LogLevel::Info, "dev build-config debug: save Debug for next dev_auto_reload start"});
            appendUiLog({LogLevel::Info, "dev build-config release: save Release for next dev_auto_reload start"});
            appendUiLog({LogLevel::Info, "quit: close the game"});
            return;
        }

        queueCommand(command);
    }

    void queueCommand(const std::string& command)
    {
        addHistory(command);
        appendUiLog({LogLevel::Command, "> " + command});
        {
            std::lock_guard<std::mutex> lock(mutex);
            commandQueue.push_back(command);
        }
    }

    void addHistory(const std::string& command)
    {
        auto existing = std::find(history.begin(), history.end(), command);
        if (existing != history.end()) {
            history.erase(existing);
        }
        history.insert(history.begin(), command);
        if (history.size() > MaxHistory) {
            history.pop_back();
        }

        SendMessageW(commandCombo, CB_RESETCONTENT, 0, 0);
        for (const std::string& item : history) {
            std::wstring wide = utf8ToWide(item);
            SendMessageW(commandCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
        }
    }

    std::string lowerAscii(std::string text) const
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return text;
    }

    void drainPendingLogs()
    {
        std::deque<LogEntry> logs;
        {
            std::lock_guard<std::mutex> lock(mutex);
            logs.swap(pendingLogs);
        }

        for (const LogEntry& log : logs) {
            appendUiLog(log);
        }
    }

    void appendUiLog(const LogEntry& entry)
    {
        logLines.push_back(entry);
        while (logLines.size() > MaxLogLines) {
            logLines.pop_front();
        }

        if (paused) {
            return;
        }

        if (logLines.size() == MaxLogLines) {
            rebuildLogText();
            return;
        }

        appendLineToEdit(entry);
    }

    std::wstring formatLog(const LogEntry& entry) const
    {
        std::wstring line = L"[";
        line += timestampText();
        line += L"] [";
        line += levelName(entry.level);
        line += L"] ";
        line += utf8ToWide(entry.message);
        line += L"\r\n";
        return line;
    }

    void appendLineToEdit(const LogEntry& entry)
    {
        const std::wstring line = formatLog(entry);
        const int end = GetWindowTextLengthW(logEdit);
        SendMessageW(logEdit, EM_SETSEL, end, end);
        CHARFORMAT2W format{};
        format.cbSize = sizeof(format);
        format.dwMask = CFM_COLOR;
        format.crTextColor = levelColor(entry.level);
        SendMessageW(logEdit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
        SendMessageW(logEdit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
        if (autoScroll) {
            SendMessageW(logEdit, EM_SCROLLCARET, 0, 0);
        }
    }

    void rebuildLogText()
    {
        SetWindowTextW(logEdit, L"");
        for (const LogEntry& entry : logLines) {
            appendLineToEdit(entry);
        }
        if (autoScroll) {
            scrollLogToBottom();
        }
    }

    void scrollLogToBottom()
    {
        if (!logEdit) {
            return;
        }
        SendMessageW(logEdit, WM_VSCROLL, SB_BOTTOM, 0);
        const int end = GetWindowTextLengthW(logEdit);
        SendMessageW(logEdit, EM_SETSEL, end, end);
        SendMessageW(logEdit, EM_SCROLLCARET, 0, 0);
    }

    void clearLogs()
    {
        logLines.clear();
        SetWindowTextW(logEdit, L"");
    }

    void copyLogsToClipboard()
    {
        const int length = GetWindowTextLengthW(logEdit);
        if (length <= 0 || !OpenClipboard(hwnd)) {
            return;
        }

        std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
        GetWindowTextW(logEdit, text.data(), length + 1);
        text.resize(static_cast<std::size_t>(length));

        EmptyClipboard();
        const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (memory) {
            void* data = GlobalLock(memory);
            if (data) {
                std::memcpy(data, text.c_str(), bytes);
                GlobalUnlock(memory);
                SetClipboardData(CF_UNICODETEXT, memory);
            } else {
                GlobalFree(memory);
            }
        }
        CloseClipboard();
    }

    void toggleWindow()
    {
        if (!hwnd) {
            return;
        }

        if (IsWindowVisible(hwnd)) {
            ShowWindow(hwnd, SW_HIDE);
            return;
        }

        ShowWindow(hwnd, SW_SHOWNORMAL);
        PostMessageW(hwnd, WM_DEBUG_SCROLL_TO_BOTTOM, 0, 0);
        SetForegroundWindow(hwnd);
        SetFocus(commandCombo);
    }

    void DestroyWindowHandles()
    {
        if (logFont) {
            DeleteObject(logFont);
            logFont = nullptr;
        }
        if (uiFont) {
            DeleteObject(uiFont);
            uiFont = nullptr;
        }
        if (richEditLibrary) {
            FreeLibrary(richEditLibrary);
            richEditLibrary = nullptr;
        }
        hwnd = nullptr;
        logEdit = nullptr;
        tabControl = nullptr;
        commandCombo = nullptr;
        runButton = nullptr;
        clearButton = nullptr;
        copyButton = nullptr;
        pauseButton = nullptr;
        autoScrollCheck = nullptr;
    }

    std::thread uiThread;
    std::mutex mutex;
    std::condition_variable readyCv;
    HWND hwnd = nullptr;
    HWND logEdit = nullptr;
    HWND tabControl = nullptr;
    HWND commandCombo = nullptr;
    HWND runButton = nullptr;
    HWND clearButton = nullptr;
    HWND copyButton = nullptr;
    HWND pauseButton = nullptr;
    HWND autoScrollCheck = nullptr;
    HFONT logFont = nullptr;
    HFONT uiFont = nullptr;
    HMODULE richEditLibrary = nullptr;
    DebugConsoleLayout layoutDefinition;
    std::vector<DebugTabUi> debugTabs;
    std::unordered_map<int, std::string> debugCommandByControlId;
    std::deque<LogEntry> pendingLogs;
    std::deque<LogEntry> logLines;
    std::deque<std::string> commandQueue;
    std::vector<std::string> history;
    bool running = false;
    bool ready = false;
    bool shuttingDown = false;
    bool paused = false;
    bool autoScroll = true;
    int currentTabIndex = 0;
#endif
};

DebugConsole::DebugConsole()
    : impl_(std::make_unique<Impl>())
{
}

DebugConsole::~DebugConsole()
{
    shutdown();
}

void DebugConsole::initialize()
{
    impl_->initialize();
}

void DebugConsole::shutdown()
{
    impl_->shutdown();
}

void DebugConsole::toggleVisible()
{
    impl_->toggleVisible();
}

void DebugConsole::appendLog(LogLevel level, std::string_view message)
{
    impl_->appendLog(level, message);
}

std::optional<std::string> DebugConsole::pollCommand()
{
    return impl_->pollCommand();
}

}
