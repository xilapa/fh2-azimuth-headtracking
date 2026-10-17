// FH2HeadTracking.exe - loads FH2HeadTracking.dll into Azimuth.
//
//   FH2HeadTracking.exe                       watch: inject into any azimuth.exe that starts
//   FH2HeadTracking.exe "<path to>\azimuth.exe"  start Azimuth, inject, exit when it closes
//
// Nothing is written to the Azimuth or game folders.
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <cstdio>
#include <set>
#include <string>
#include <vector>
#include "../dll/config.h"

static std::wstring selfDir() {
    wchar_t p[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, p, MAX_PATH);
    std::wstring s(p, n);
    return s.substr(0, s.find_last_of(L"\\/"));
}

static std::vector<DWORD> findAzimuth() {
    std::vector<DWORD> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) do {
        if (_wcsicmp(pe.szExeFile, L"azimuth.exe") == 0) out.push_back(pe.th32ProcessID);
    } while (Process32NextW(snap, &pe));
    CloseHandle(snap);
    return out;
}

static bool hasModule(HANDLE proc, const std::wstring& dllPath) {
    HMODULE mods[1024]; DWORD need = 0;
    if (!EnumProcessModulesEx(proc, mods, sizeof(mods), &need, LIST_MODULES_64BIT)) return false;
    for (DWORD i = 0; i < need / sizeof(HMODULE); ++i) {
        wchar_t name[MAX_PATH];
        if (GetModuleFileNameExW(proc, mods[i], name, MAX_PATH) && _wcsicmp(name, dllPath.c_str()) == 0) return true;
    }
    return false;
}

static bool inject(DWORD pid, const std::wstring& dllPath) {
    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                              PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!proc) { wprintf(L"  could not open azimuth.exe (pid %lu), error %lu\n", pid, GetLastError()); return false; }
    if (hasModule(proc, dllPath)) { wprintf(L"  pid %lu already has head tracking\n", pid); CloseHandle(proc); return true; }
    size_t bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    bool ok = false;
    if (remote && WriteProcessMemory(proc, remote, dllPath.c_str(), bytes, nullptr)) {
        auto loadLib = reinterpret_cast<LPTHREAD_START_ROUTINE>(
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
        HANDLE t = CreateRemoteThread(proc, nullptr, 0, loadLib, remote, 0, nullptr);
        if (t) {
            WaitForSingleObject(t, 15000);
            DWORD code = 0; GetExitCodeThread(t, &code);
            ok = code != 0;
            CloseHandle(t);
        }
    }
    if (remote) VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);
    wprintf(ok ? L"  head tracking loaded into azimuth.exe (pid %lu)\n"
               : L"  FAILED to load into azimuth.exe (pid %lu) - antivirus? run as the same user as Azimuth\n", pid);
    return ok;
}

static std::wstring keyName(int vk) {
    if (vk >= 0x70 && vk <= 0x7B) return L"F" + std::to_wstring(vk - 0x70 + 1);
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) return std::wstring(1, (wchar_t)vk);
    switch (vk) {
    case VK_END: return L"End";
    case VK_HOME: return L"Home";
    case VK_PRIOR: return L"Page Up";
    case VK_NEXT: return L"Page Down";
    case VK_INSERT: return L"Insert";
    case VK_DELETE: return L"Delete";
    }
    wchar_t hex[16]; swprintf(hex, 16, L"key 0x%02X", vk);
    return hex;
}

// "End, Ctrl+Shift+Y, controller L3+R3" - whatever is set; "off" when nothing is.
static std::wstring bindingText(int key, int chordKey, const wchar_t* buttons) {
    std::wstring s;
    auto add = [&](const std::wstring& t) { s += (s.empty() ? L"" : L", ") + t; };
    if (key) add(keyName(key));
    if (chordKey) add(L"Ctrl+Shift+" + keyName(chordKey));
    if (buttons && *buttons) add(std::wstring(L"controller ") + buttons);
    return s.empty() ? L"off" : s;
}

int wmain(int argc, wchar_t** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::wstring dll = selfDir() + L"\\FH2HeadTracking.dll";
    if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES) {
        wprintf(L"FH2HeadTracking.dll not found next to this program:\n  %ls\n", dll.c_str());
        Sleep(5000);
        return 1;
    }
    // Also writes the default INI if missing, so it can be edited before the game starts.
    std::wstring ini = selfDir() + L"\\FH2HeadTracking.ini";
    fh2::Config cfg = fh2::loadConfig(ini);
    wchar_t buttons[128];
    GetPrivateProfileStringW(L"Hotkeys", L"RecenterButtons", L"L3+R3", buttons, 128, ini.c_str());

    wprintf(L"FH2 Head Tracking for Azimuth\n");
    switch (cfg.source) {
    case fh2::Source::Udp:
        wprintf(L"  Input:    OpenTrack \"UDP over network\", %hs port %d\n", cfg.bindAddress.c_str(), cfg.udpPort);
        break;
    case fh2::Source::FreeTrack:
        wprintf(L"  Input:    FreeTrack/TrackIR shared memory (Beam, or OpenTrack \"freetrack 2.0 Enhanced\")\n");
        break;
    default:
        wprintf(L"  Input:    auto - FreeTrack/TrackIR shared memory when present,\n"
                L"            otherwise OpenTrack \"UDP over network\", %hs port %d\n", cfg.bindAddress.c_str(), cfg.udpPort);
    }
    wprintf(L"  Toggle:   %ls\n", bindingText(cfg.toggleKey, cfg.chordToggleKey, nullptr).c_str());
    wprintf(L"  Recenter: %ls\n", bindingText(cfg.recenterKey, cfg.chordRecenterKey, buttons).c_str());
    wprintf(L"  Settings: %ls\n  Log:      FH2HeadTracking.log\n\n", ini.c_str());

    bool launched = false;
    if (argc > 1) {
        std::wstring exe = argv[1];
        std::wstring dir = exe.substr(0, exe.find_last_of(L"\\/"));
        STARTUPINFOW si{}; si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        std::wstring cmd = L"\"" + exe + L"\"";
        if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) {
            wprintf(L"Could not start %ls (error %lu)\n", exe.c_str(), GetLastError());
            Sleep(5000);
            return 1;
        }
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        launched = true;
        wprintf(L"Started Azimuth.\n");
    } else {
        wprintf(L"Waiting for Azimuth to start... (close this window to stop)\n");
    }

    std::set<DWORD> done;
    int idleSeconds = 0;
    for (;;) {
        auto pids = findAzimuth();
        for (DWORD pid : pids) {
            if (done.count(pid)) continue;
            Sleep(1000);                       // let the new process finish starting up
            inject(pid, dll);
            done.insert(pid);
        }
        if (launched) {
            idleSeconds = pids.empty() ? idleSeconds + 1 : 0;
            if (idleSeconds > 10) { wprintf(L"Azimuth closed. Bye.\n"); return 0; }
        }
        Sleep(1000);
    }
}
