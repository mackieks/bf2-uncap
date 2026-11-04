// swbf2_uncap dinput8.dll proxy.
// Removes the hardcoded 5 ms (200 fps) render-thread frame floor in STAR WARS Battlefront II (2017)
// by writing the limiter's interval (a data value) from inside the game process. No code is patched.
// All real DirectInput exports are forwarded to the system dinput8.dll.
//
// Config (optional): swbf2_uncap.ini next to the DLL
//   [uncap]
//   Interval=0        ; QPC ticks per frame floor. 0 = uncapped, 50000 = stock 200 fps, 25000 = 400 fps
//   Enabled=1
// Log: swbf2_uncap.log next to the DLL.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>

#define SWBF2_UNCAP_VERSION "1.0.0"

// ---- Addresses for game build MD5 76a8ee9a94ef97360a9a3e5974fd5753 (fixed base 0x140000000) ----
static const uint64_t kImageBase     = 0x140000000ULL;
static const uint64_t kOwnerGlobal   = 0x144090F70ULL; // render pacing owner (static object)
static const uint64_t kOwnerVtable   = 0x143164610ULL;
static const uint64_t kOwnerListOff  = 0x8A8;          // owner -> pacing entry[0]
static const uint64_t kEntrySubOff   = 0x50;           // entry -> limiter
static const uint64_t kLimiterVtable = 0x143162C68ULL;
static const uint64_t kIntervalOff   = 0x7060;         // int64 QPC ticks

static wchar_t g_dir[MAX_PATH];
static uint64_t g_interval = 0;

static void logf(const char* fmt, ...) {
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%sswbf2_uncap.log", g_dir);
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") != 0 || !f) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(f, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list a; va_start(a, fmt); vfprintf(f, fmt, a); va_end(a);
    fputc('\n', f);
    fclose(f);
}

// Guarded read: returns false instead of crashing on a bad pointer.
static bool rd(uint64_t addr, uint64_t& out) {
    __try { out = *reinterpret_cast<volatile uint64_t*>(addr); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static uint64_t resolve_limiter() {
    uint64_t vt, entry, lim, lvt;
    if (!rd(kOwnerGlobal, vt) || vt != kOwnerVtable) return 0;
    if (!rd(kOwnerGlobal + kOwnerListOff, entry) || entry < 0x10000) return 0;
    if (!rd(entry + kEntrySubOff, lim) || lim < 0x10000) return 0;
    if (!rd(lim, lvt) || lvt != kLimiterVtable) return 0;
    return lim;
}

static DWORD WINAPI worker(void*) {
    // Wrong executable or base => do nothing.
    HMODULE exe = GetModuleHandleW(nullptr);
    wchar_t exePath[MAX_PATH]; GetModuleFileNameW(exe, exePath, MAX_PATH);
    const wchar_t* exeName = wcsrchr(exePath, L'\\'); exeName = exeName ? exeName + 1 : exePath;
    if ((uint64_t)exe != kImageBase || _wcsicmp(exeName, L"starwarsbattlefrontii.exe") != 0) {
        logf("host is %ls at %p: not the supported game executable, staying idle", exeName, exe);
        return 0;
    }
    logf("swbf2_uncap " SWBF2_UNCAP_VERSION " loaded, target interval %llu ticks", (unsigned long long)g_interval);
    uint64_t lastLim = 0;
    DWORD wrongBuildSince = 0;
    for (;;) {
        uint64_t lim = resolve_limiter();
        if (!lim) {
            uint64_t vt = 0;
            if (rd(kOwnerGlobal, vt) && vt != 0 && vt != kOwnerVtable) {
                // Non-zero but unexpected => probably a different game build. Give the game
                // time to finish start-up, then stop for good.
                if (!wrongBuildSince) wrongBuildSince = GetTickCount();
                else if (GetTickCount() - wrongBuildSince > 180000) {
                    logf("unexpected data at owner global (0x%llX): unsupported game build, stopping",
                         (unsigned long long)vt);
                    return 0;
                }
            } else wrongBuildSince = 0;
        } else {
            wrongBuildSince = 0;
            if (lim != lastLim) { logf("limiter found at 0x%llX", (unsigned long long)lim); lastLim = lim; }
            volatile int64_t* iv = reinterpret_cast<volatile int64_t*>(lim + kIntervalOff);
            uint64_t cur;
            if (rd((uint64_t)iv, cur) && cur != g_interval && cur <= 10000000ULL) {
                // Same atomic exchange the game's own setter uses.
                InterlockedExchange64(iv, (int64_t)g_interval);
                logf("interval %llu -> %llu", (unsigned long long)cur, (unsigned long long)g_interval);
            }
        }
        Sleep(500);
    }
}

// ---- dinput8 forwarding ----
static HMODULE g_real = nullptr;
static FARPROC real_proc(const char* name) {
    if (!g_real) {
        wchar_t sys[MAX_PATH]; GetSystemDirectoryW(sys, MAX_PATH);
        wcscat_s(sys, L"\\dinput8.dll");
        g_real = LoadLibraryW(sys);
    }
    return g_real ? GetProcAddress(g_real, name) : nullptr;
}

extern "C" {
HRESULT WINAPI Proxy_DirectInput8Create(HINSTANCE h, DWORD v, REFIID r, LPVOID* o, LPUNKNOWN u) {
    auto f = (HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN))real_proc("DirectInput8Create");
    return f ? f(h, v, r, o, u) : E_FAIL;
}
HRESULT WINAPI Proxy_DllCanUnloadNow() {
    auto f = (HRESULT(WINAPI*)())real_proc("DllCanUnloadNow"); return f ? f() : S_FALSE;
}
HRESULT WINAPI Proxy_DllGetClassObject(REFCLSID c, REFIID r, LPVOID* o) {
    auto f = (HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*))real_proc("DllGetClassObject");
    return f ? f(c, r, o) : E_FAIL;
}
HRESULT WINAPI Proxy_DllRegisterServer() {
    auto f = (HRESULT(WINAPI*)())real_proc("DllRegisterServer"); return f ? f() : E_FAIL;
}
HRESULT WINAPI Proxy_DllUnregisterServer() {
    auto f = (HRESULT(WINAPI*)())real_proc("DllUnregisterServer"); return f ? f() : E_FAIL;
}
LPCVOID WINAPI Proxy_GetdfDIJoystick() {
    auto f = (LPCVOID(WINAPI*)())real_proc("GetdfDIJoystick"); return f ? f() : nullptr;
}
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        GetModuleFileNameW(inst, g_dir, MAX_PATH);
        if (wchar_t* s = wcsrchr(g_dir, L'\\')) s[1] = 0;
        wchar_t ini[MAX_PATH]; swprintf_s(ini, L"%sswbf2_uncap.ini", g_dir);
        wchar_t buf[32];
        GetPrivateProfileStringW(L"uncap", L"Interval", L"0", buf, 32, ini);
        g_interval = _wcstoui64(buf, nullptr, 0);
        if (g_interval > 10000000ULL) g_interval = 0;
        if (GetPrivateProfileIntW(L"uncap", L"Enabled", 1, ini)) {
            HANDLE t = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
            if (t) { SetThreadPriority(t, THREAD_PRIORITY_BELOW_NORMAL); CloseHandle(t); }
        }
    }
    return TRUE;
}
