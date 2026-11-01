#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <cstdio>

static HWND g_edit, g_log;
static std::wstring g_dll;

static void Log(const wchar_t* s) {
    int n = GetWindowTextLengthW(g_log);
    SendMessageW(g_log, EM_SETSEL, n, n);
    SendMessageW(g_log, EM_REPLACESEL, FALSE, (LPARAM)s);
    SendMessageW(g_log, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
}

static DWORD FindPid(const wchar_t* exe) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ sizeof(pe) };
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) do {
        if (!_wcsicmp(pe.szExeFile, exe)) { pid = pe.th32ProcessID; break; }
    } while (Process32NextW(snap, &pe));
    CloseHandle(snap);
    return pid;
}

// Classic LoadLibrary injection. If Roblox rejects it, swap for a manual
// mapper (PE sections -> VirtualAllocEx, fix relocs/imports, call entry).
static bool Inject(DWORD pid, const std::wstring& dll) {
    HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!proc) { Log(L"OpenProcess failed — run as admin."); return false; }

    SIZE_T bytes = (dll.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(proc, nullptr, bytes,
                                  MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { Log(L"VirtualAllocEx failed."); CloseHandle(proc); return false; }

    bool ok = false;
    if (WriteProcessMemory(proc, remote, dll.c_str(), bytes, nullptr)) {
        auto load = (LPTHREAD_START_ROUTINE)GetProcAddress(
            GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
        HANDLE th = CreateRemoteThread(proc, nullptr, 0, load, remote, 0, nullptr);
        if (th) {
            WaitForSingleObject(th, 15000);
            DWORD code = 0;
            GetExitCodeThread(th, &code);
            ok = (code != 0);
            CloseHandle(th);
        }
    }
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);
    return ok;
}

static void SendScript(const std::string& src) {
    HANDLE pipe = CreateFileW(L"\\\\.\\pipe\\venice_exec", GENERIC_WRITE, 0,
                              nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) { Log(L"Pipe not found — inject first."); return; }
    DWORD w = 0;
    WriteFile(pipe, src.data(), (DWORD)src.size(), &w, nullptr);
    CloseHandle(pipe);
}

static void OnAttach() {
    DWORD pid = FindPid(L"RobloxPlayerBeta.exe");
    if (!pid) { Log(L"Roblox not running — join a game first."); return; }
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring dll = self;
    dll = dll.substr(0, dll.find_last_of(L'\\') + 1) + L"VeniceCore.dll";
    Log(Inject(pid, dll) ? L"Attached." : L"Injection failed.");
}

static void OnExecute() {
    int n = GetWindowTextLengthW(g_edit);
    std::wstring w(n + 1, 0);
    GetWindowTextW(g_edit, w.data(), n + 1);
    int need = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), n, nullptr, 0, nullptr, nullptr);
    std::string s(need, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), n, s.data(), need, nullptr, nullptr);
    SendScript(s);
    Log(L"Sent.");
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE:
        CreateWindowW(L"BUTTON", L"Attach", WS_CHILD | WS_VISIBLE, 10, 10, 90, 28, h, (HMENU)1, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Execute", WS_CHILD | WS_VISIBLE, 110, 10, 90, 28, h, (HMENU)2, nullptr, nullptr);
        g_edit = CreateWindowW(L"EDIT", L"print(\"hello\")",
                    WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                    10, 48, 560, 300, h, nullptr, nullptr, nullptr);
        g_log = CreateWindowW(L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                    10, 358, 560, 110, h, nullptr, nullptr, nullptr);
        SendMessageW(g_edit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        SendMessageW(g_log,  WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        break;
    case WM_COMMAND:
        if (LOWORD(w) == 1) OnAttach();
        if (LOWORD(w) == 2) OnExecute();
        break;
    case WM_DESTROY: PostQuitMessage(0); break;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc; wc.hInstance = inst;
    wc.lpszClassName = L"VeniceExec"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);
    CreateWindowW(L"VeniceExec", L"Venice Exec", WS_OVERLAPPEDWINDOW,
                  CW_USEDEFAULT, CW_USEDEFAULT, 600, 520, nullptr, nullptr, inst, nullptr);
    ShowWindow(GetActiveWindow(), show);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return 0;
}