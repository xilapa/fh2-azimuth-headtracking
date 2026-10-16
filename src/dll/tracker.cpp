#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <atomic>
#include <cstring>
#include "tracker.h"
#include "log.h"

namespace fh2 {

namespace {
Config            g_cfg;
SRWLOCK           g_lock = SRWLOCK_INIT;
CameraOffsets     g_published;         // guarded by g_lock
bool              g_publishedValid = false;
std::atomic<bool> g_enabled{true};
std::atomic<bool> g_recenterRequest{false};

double nowSeconds() {
    static LARGE_INTEGER freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
    LARGE_INTEGER t; QueryPerformanceCounter(&t);
    return double(t.QuadPart) / double(freq.QuadPart);
}

void publish(const CameraOffsets& o, bool valid) {
    AcquireSRWLockExclusive(&g_lock);
    g_published = o;
    g_publishedValid = valid;
    ReleaseSRWLockExclusive(&g_lock);
}

DWORD WINAPI udpThread(void*) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { LOG("udp: WSAStartup failed"); return 1; }
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) { LOG("udp: socket() failed %d", WSAGetLastError()); return 1; }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)g_cfg.udpPort);
    if (inet_pton(AF_INET, g_cfg.bindAddress.c_str(), &addr.sin_addr) != 1) {
        LOG("udp: bad BindAddress '%s', using 127.0.0.1", g_cfg.bindAddress.c_str());
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    }
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) != 0) {
        LOG("udp: bind %s:%d failed (%d). Is another program using the port?",
            g_cfg.bindAddress.c_str(), g_cfg.udpPort, WSAGetLastError());
        return 1;
    }
    DWORD timeoutMs = 100;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));
    LOG("udp: listening on %s:%d", g_cfg.bindAddress.c_str(), g_cfg.udpPort);

    RawPose center, smoothed;
    bool haveCenter = false, haveSmoothed = false, loggedFirst = false;
    double lastPacket = -1e9, lastStep = nowSeconds();
    bool wasStale = true;

    for (;;) {
        double buf[6];
        int n = recv(s, (char*)buf, sizeof(buf), 0);
        double t = nowSeconds();
        if (n == (int)sizeof(buf)) {
            RawPose p{buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]};
            if (!loggedFirst) {
                LOG("udp: first packet x%.1f y%.1f z%.1f yaw%.1f pitch%.1f roll%.1f",
                    p.x, p.y, p.z, p.yaw, p.pitch, p.roll);
                loggedFirst = true;
            }
            double dt = t - lastStep; lastStep = t;
            if (!haveSmoothed || dt > 0.25) { smoothed = p; haveSmoothed = true; }
            else smoothPose(smoothed, p, smoothFactor(g_cfg.smoothing, dt));
            lastPacket = t;
            if (g_recenterRequest.exchange(false) || !haveCenter) {
                // OpenTrack already centres; this is an extra in-game recentre.
                // The first centre is identity so OpenTrack's own centre is respected.
                if (haveCenter) { center = smoothed; LOG("recentred"); }
                haveCenter = true;
            }
            if (wasStale) { LOG("udp: tracking data flowing"); wasStale = false; }
            publish(mapPose(smoothed, center, g_cfg.map), true);
        } else {
            if (g_recenterRequest.exchange(false) && haveSmoothed) { center = smoothed; LOG("recentred"); }
            if (!wasStale && (t - lastPacket) * 1000.0 > g_cfg.staleTimeoutMs) {
                LOG("udp: no data for %d ms, view returns to centre", g_cfg.staleTimeoutMs);
                wasStale = true;
                publish(CameraOffsets{}, false);
            }
        }
    }
}

bool pressed(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

DWORD WINAPI hotkeyThread(void*) {
    bool prevToggle = false, prevRecenter = false;
    for (;;) {
        Sleep(15);
        bool chord = pressed(VK_CONTROL) && pressed(VK_SHIFT);
        bool toggle   = pressed(g_cfg.toggleKey)   || (chord && pressed(g_cfg.chordToggleKey));
        bool recenter = pressed(g_cfg.recenterKey) || (chord && pressed(g_cfg.chordRecenterKey));
        if (toggle && !prevToggle) {
            bool e = !g_enabled.load();
            g_enabled = e;
            LOG("head tracking %s", e ? "ON" : "OFF");
        }
        if (recenter && !prevRecenter) g_recenterRequest = true;
        prevToggle = toggle; prevRecenter = recenter;
    }
}
} // namespace

void Tracker::start(const Config& cfg) {
    g_cfg = cfg;
    g_enabled = cfg.enableOnStartup;
    CloseHandle(CreateThread(nullptr, 0, udpThread, nullptr, 0, nullptr));
    CloseHandle(CreateThread(nullptr, 0, hotkeyThread, nullptr, 0, nullptr));
}

bool Tracker::read(CameraOffsets& out) {
    if (!g_enabled.load(std::memory_order_relaxed)) return false;
    AcquireSRWLockShared(&g_lock);
    bool v = g_publishedValid;
    if (v) out = g_published;
    ReleaseSRWLockShared(&g_lock);
    return v;
}

} // namespace fh2
