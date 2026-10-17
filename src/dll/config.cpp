#include "config.h"
#include "config_file.h"
#include "log.h"
#include <cstdio>
#include <cstdlib>
#include <cctype>

namespace fh2 {

// "L3+R3" -> XINPUT_GAMEPAD_* mask
static unsigned parseButtons(const std::string& spec) {
    static const struct { const char* name; unsigned mask; } kNames[] = {
        {"UP", 0x0001}, {"DOWN", 0x0002}, {"LEFT", 0x0004}, {"RIGHT", 0x0008},
        {"START", 0x0010}, {"BACK", 0x0020}, {"L3", 0x0040}, {"R3", 0x0080},
        {"LB", 0x0100}, {"RB", 0x0200}, {"A", 0x1000}, {"B", 0x2000}, {"X", 0x4000}, {"Y", 0x8000},
    };
    unsigned mask = 0;
    size_t i = 0;
    while (i < spec.size()) {
        size_t j = spec.find('+', i);
        if (j == std::string::npos) j = spec.size();
        std::string tok;
        for (size_t k = i; k < j; ++k) if (spec[k] != ' ') tok += (char)toupper((unsigned char)spec[k]);
        bool known = tok.empty();
        for (auto& n : kNames) if (tok == n.name) { mask |= n.mask; known = true; }
        if (!known) LOG("config: unknown controller button '%s' in RecenterButtons", tok.c_str());
        i = j + 1;
    }
    return mask;
}

static std::string readStr(const wchar_t* ini, const char* sec, const char* key, const char* def) {
    char buf[256];
    // ANSI path variant is fine for the key/section names; convert path once.
    char path[MAX_PATH * 3];
    WideCharToMultiByte(CP_ACP, 0, ini, -1, path, sizeof(path), nullptr, nullptr);
    GetPrivateProfileStringA(sec, key, def, buf, sizeof(buf), path);
    return buf;
}
static double readF(const wchar_t* ini, const char* sec, const char* key, double def) {
    char d[64]; snprintf(d, sizeof(d), "%g", def);
    return atof(readStr(ini, sec, key, d).c_str());
}
static int readI(const wchar_t* ini, const char* sec, const char* key, int def) {
    char d[32]; snprintf(d, sizeof(d), "%d", def);
    return (int)strtol(readStr(ini, sec, key, d).c_str(), nullptr, 0);
}
static void readOrder(const wchar_t* ini, const char* key, int out[3]) {
    std::string s = readStr(ini, "Advanced", key, "0,1,2");
    int a = 0, b = 1, c = 2;
    if (sscanf(s.c_str(), "%d,%d,%d", &a, &b, &c) == 3 && a >= 0 && a < 3 && b >= 0 && b < 3 &&
        c >= 0 && c < 3 && a != b && b != c && a != c) {
        out[0] = a; out[1] = b; out[2] = c;
    } else {
        LOG("config: %s='%s' is not a permutation of 0,1,2; using 0,1,2", key, s.c_str());
    }
}
static AxisCfg readAxis(const wchar_t* ini, const char* sec, const char* name, float defLimit) {
    AxisCfg a;
    char k[64];
    snprintf(k, sizeof(k), "%sGain", name);   a.gain   = (float)readF(ini, sec, k, 1.0);
    snprintf(k, sizeof(k), "%sInvert", name); a.invert = readI(ini, sec, k, 0) != 0;
    snprintf(k, sizeof(k), "%sLimit", name);  a.limit  = (float)readF(ini, sec, k, defLimit);
    if (a.limit < 0) a.limit = 0;
    return a;
}

Config loadConfig(const std::wstring& iniPath) {
    ensureConfigFile(iniPath);
    const wchar_t* ini = iniPath.c_str();
    Config c;
    std::string src = readStr(ini, "Input", "Source", "auto");
    c.source = src == "udp" ? Source::Udp : src == "freetrack" ? Source::FreeTrack : Source::Auto;
    c.diagnostics     = readI(ini, "Input", "Diagnostics", 0) != 0;
    c.udpPort         = readI(ini, "Network", "UdpPort", 4242);
    c.bindAddress     = readStr(ini, "Network", "BindAddress", "127.0.0.1");
    c.enableOnStartup = readI(ini, "General", "EnableOnStartup", 1) != 0;
    c.cockpitOnly     = readI(ini, "General", "CockpitOnly", 1) != 0;
    c.staleTimeoutMs  = readI(ini, "General", "StaleTimeoutMs", 500);
    c.smoothing       = readF(ini, "General", "Smoothing", 0.0);
    c.toggleKey        = readI(ini, "Hotkeys", "ToggleKey", VK_END);
    c.recenterKey      = readI(ini, "Hotkeys", "RecenterKey", 0);
    c.chordToggleKey   = readI(ini, "Hotkeys", "ChordToggleKey", 'Y');
    c.chordRecenterKey = readI(ini, "Hotkeys", "ChordRecenterKey", 0);
    c.recenterButtons  = parseButtons(readStr(ini, "Hotkeys", "RecenterButtons", "L3+R3"));
    c.map.yaw   = readAxis(ini, "Rotation", "Yaw", 90.0f);
    c.map.pitch = readAxis(ini, "Rotation", "Pitch", 40.0f);
    c.map.roll  = readAxis(ini, "Rotation", "Roll", 20.0f);
    c.map.positionEnabled = readI(ini, "Position", "Enabled", 1) != 0;
    c.map.x = readAxis(ini, "Position", "X", 0.10f);
    c.map.y = readAxis(ini, "Position", "Y", 0.06f);
    c.map.z = readAxis(ini, "Position", "Z", 0.08f);
    readOrder(ini, "RotOrder", c.map.rotOrder);
    readOrder(ini, "PosOrder", c.map.posOrder);
    LOG("config: source %s diagnostics %d recenter buttons 0x%04x", src.c_str(), c.diagnostics, c.recenterButtons);
    LOG("config: port %d bind %s enabled %d cockpitOnly %d smoothing %.2f",
        c.udpPort, c.bindAddress.c_str(), c.enableOnStartup, c.cockpitOnly, c.smoothing);
    LOG("config: yaw g%.2f i%d l%.0f  pitch g%.2f i%d l%.0f  roll g%.2f i%d l%.0f",
        c.map.yaw.gain, c.map.yaw.invert, c.map.yaw.limit, c.map.pitch.gain, c.map.pitch.invert,
        c.map.pitch.limit, c.map.roll.gain, c.map.roll.invert, c.map.roll.limit);
    LOG("config: pos %d  x g%.2f i%d l%.2f  y g%.2f i%d l%.2f  z g%.2f i%d l%.2f",
        c.map.positionEnabled, c.map.x.gain, c.map.x.invert, c.map.x.limit, c.map.y.gain,
        c.map.y.invert, c.map.y.limit, c.map.z.gain, c.map.z.invert, c.map.z.limit);
    return c;
}

} // namespace fh2
