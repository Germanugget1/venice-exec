#include <windows.h>
#include "sigs.h"
extern "C" {
#include "lua.h"
#include "lualib.h"
}
#include <string>
#include <cstring>
#include <vector>

static HANDLE g_pipe = nullptr;

static void ToClient(const char* s) {
    if (!g_pipe) return;
    DWORD w;
    WriteFile(g_pipe, s, (DWORD)strlen(s), &w, nullptr);
}

static int l_print(lua_State* L) {
    int n = lua_gettop(L);
    std::string out;
    for (int i = 1; i <= n; ++i) {
        size_t len; const char* s = luaL_tolstring(L, i, &len);
        if (i > 1) out += '\t';
        out += s;
        lua_pop(L, 1);
    }
    out += '\n';
    ToClient(out.c_str());
    return 0;
}

static std::string Dir() {
    wchar_t p[MAX_PATH];
    GetEnvironmentVariableW(L"LOCALAPPDATA", p, MAX_PATH);
    std::wstring d = std::wstring(p) + L"\\VeniceExec";
    CreateDirectoryW(d.c_str(), nullptr);
    char o[MAX_PATH * 3]; WideCharToMultiByte(CP_UTF8, 0, d.c_str(), -1, o, sizeof o, nullptr, nullptr);
    return std::string(o) + "\\";
}

static int l_readfile(lua_State* L) {
    std::string path = Dir() + luaL_checkstring(L, 1);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { lua_pushnil(L); return 1; }
    std::string buf; char c[4096]; size_t r;
    while ((r = fread(c, 1, sizeof c, f)) > 0) buf.append(c, r);
    fclose(f);
    lua_pushlstring(L, buf.data(), buf.size());
    return 1;
}

static int l_writefile(lua_State* L) {
    std::string path = Dir() + luaL_checkstring(L, 1);
    size_t len; const char* s = luaL_checklstring(L, 2, &len);
    FILE* f = fopen(path.c_str(), "wb");
    if (f) { fwrite(s, 1, len, f); fclose(f); }
    return 0;
}

static int l_isfile(lua_State* L) {
    std::string path = Dir() + luaL_checkstring(L, 1);
    FILE* f = fopen(path.c_str(), "rb");
    lua_pushboolean(L, f != nullptr);
    if (f) fclose(f);
    return 1;
}

// TODO: resolve DataModel via TaskScheduler and expose `game`.
// Walk off::Jobs for the job named "Heartbeat" (or RenderJob) -> its DataModel,
// then push it as userdata and map __index to the instance API.
static void PushGame(lua_State* L) { lua_newtable(L); }

static void OpenApi(lua_State* L) {static int l_getgenv(lua_State* L) {
    lua_getglobal(L, "__genv");
    return 1;
}

static void OpenApi(lua_State* L) {
    lua_pushcfunction(L, l_print, "print");         lua_setglobal(L, "print");
    lua_pushcfunction(L, l_readfile, "readfile");   lua_setglobal(L, "readfile");
    lua_pushcfunction(L, l_writefile, "writefile"); lua_setglobal(L, "writefile");
    lua_pushcfunction(L, l_isfile, "isfile");       lua_setglobal(L, "isfile");
    lua_pushcfunction(L, l_getgenv, "getgenv");     lua_setglobal(L, "getgenv");
    PushGame(L);                                    lua_setglobal(L, "game");
    lua_newtable(L);                                lua_setglobal(L, "__genv");
}

static void OpenApi(lua_State* L) {
    lua_pushcfunction(L, l_print, "print");         lua_setglobal(L, "print");
    lua_pushcfunction(L, l_readfile, "readfile");   lua_setglobal(L, "readfile");
    lua_pushcfunction(L, l_writefile, "writefile"); lua_setglobal(L, "writefile");
    lua_pushcfunction(L, l_isfile, "isfile");       lua_setglobal(L, "isfile");
    lua_pushcfunction(L, l_getgenv, "getgenv");     lua_setglobal(L, "getgenv");
    PushGame(L);                                    lua_setglobal(L, "game");
    lua_newtable(L);                                lua_setglobal(L, "__genv");
}}

static void Run(lua_State* L, const char* src, size_t len) {
    if (luaL_loadbuffer(L, src, len, "=exec") || lua_pcall(L, 0, 0, 0)) {
        const char* err = lua_tostring(L, -1);
        std::string m = "[error] "; m += err ? err : "unknown"; m += '\n';
        ToClient(m.c_str());
        lua_pop(L, 1);
    }
}

static DWORD WINAPI PipeThread(LPVOID) {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    OpenApi(L);

    for (;;) {
        g_pipe = CreateNamedPipeW(L"\\\\.\\pipe\\venice_exec",
                    PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                    PIPE_UNLIMITED_INSTANCES, 1 << 16, 1 << 16, 0, nullptr);
        if (ConnectNamedPipe(g_pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) {
            std::string buf; char c[8192]; DWORD r;
            while (ReadFile(g_pipe, c, sizeof c, &r, nullptr) && r > 0) buf.append(c, r);
            Run(L, buf.data(), buf.size());
            DisconnectNamedPipe(g_pipe);
        }
        CloseHandle(g_pipe);
        g_pipe = nullptr;
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(mod);
        CreateThread(nullptr, 0, PipeThread, nullptr, 0, nullptr);
    }
    return TRUE;
}