// FH2 Head Tracking for Azimuth
// Feeds OpenTrack head pose into Forza Horizon 2's own head-tracking camera layers.
#include <windows.h>
#include <string>
#include "config.h"
#include "hook.h"
#include "log.h"
#include "tracker.h"

#ifndef FH2HT_VERSION   // set by the build from CMakeLists.txt project(VERSION)
#define FH2HT_VERSION "dev"
#endif

namespace {

std::wstring moduleDir(HMODULE m) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(m, path, MAX_PATH);
    std::wstring s(path, n);
    size_t slash = s.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : s.substr(0, slash);
}

DWORD WINAPI initThread(void* param) {
    HMODULE self = static_cast<HMODULE>(param);
    std::wstring dir = moduleDir(self);
    fh2::Log::open((dir + L"\\FH2HeadTracking.log").c_str());

    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    LOG("FH2 Head Tracking " FH2HT_VERSION " loaded into pid %lu (%ls)", GetCurrentProcessId(), exe);

    fh2::Config cfg = fh2::loadConfig(dir + L"\\FH2HeadTracking.ini");
    fh2::Tracker::start(cfg);
    fh2::installWhenReady(cfg);
    return 0;
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        // One instance per process, whichever way we were loaded (injector or ASI loader).
        wchar_t name[64];
        wsprintfW(name, L"Local\\FH2HeadTracking_%lu", GetCurrentProcessId());
        HANDLE m = CreateMutexW(nullptr, TRUE, name);
        if (m && GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(m); return TRUE; }
        // Pin ourselves: an injector's LoadLibrary thread exits, we must stay.
        HMODULE pinned;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&DllMain), &pinned);
        if (HANDLE t = CreateThread(nullptr, 0, initThread, inst, 0, nullptr)) CloseHandle(t);
    }
    return TRUE;
}
