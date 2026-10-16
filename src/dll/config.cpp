#include "config.h"
#include "log.h"
#include <cstdio>
#include <cstdlib>

namespace fh2 {

static const char kDefaultIni[] =
"; FH2 Head Tracking for Azimuth - settings. Edit, then restart the game.\n"
"\n"
"[Network]\n"
"; OpenTrack: Output = \"UDP over network\", address 127.0.0.1, port below.\n"
"UdpPort=4242\n"
"; 127.0.0.1 = this PC only. 0.0.0.0 = also accept a phone/second PC on the LAN.\n"
"BindAddress=127.0.0.1\n"
"\n"
"[General]\n"
"EnableOnStartup=1\n"
"; 1 = cockpit camera only. 0 = also hood and bumper cameras.\n"
"CockpitOnly=1\n"
"; With no packets for this long the view eases back to centre.\n"
"StaleTimeoutMs=500\n"
"; 0.0 none .. 1.0 heavy. Prefer OpenTrack's own filter (Accela) and leave 0.\n"
"Smoothing=0.0\n"
"\n"
"[Hotkeys]\n"
"; Windows virtual key codes in hex. End=0x23 Home=0x24 PgUp=0x21 PgDn=0x22 F1..F12=0x70..0x7B\n"
"ToggleKey=0x23\n"
"RecenterKey=0x24\n"
"; Same actions with Ctrl+Shift+<key>, for keyboards without a nav cluster.\n"
"ChordToggleKey=0x59\n"
"ChordRecenterKey=0x48\n"
"\n"
"; Gain multiplies your head movement. Invert flips an axis. Limit caps the\n"
"; camera offset (degrees for rotation, metres for position); Limit=0 turns\n"
"; that axis off. Sensitivity curves belong in OpenTrack's Mapping window.\n"
"[Rotation]\n"
"YawGain=1.0\n"
"YawInvert=0\n"
"YawLimit=150\n"
"PitchGain=1.0\n"
"PitchInvert=0\n"
"PitchLimit=60\n"
"RollGain=1.0\n"
"RollInvert=0\n"
"RollLimit=30\n"
"\n"
"[Position]\n"
"Enabled=1\n"
"XGain=1.0\n"
"XInvert=0\n"
"XLimit=0.25\n"
"YGain=1.0\n"
"YInvert=0\n"
"YLimit=0.15\n"
"ZGain=1.0\n"
"ZInvert=0\n"
"ZLimit=0.20\n"
"\n"
"[Advanced]\n"
"; Which component of the game's rotation Param receives yaw, pitch, roll (0,1,2).\n"
"RotOrder=0,1,2\n"
"; Which car-space component receives head X, Y, Z (0,1,2).\n"
"PosOrder=0,1,2\n";

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
    if (GetFileAttributesW(iniPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (FILE* f = _wfopen(iniPath.c_str(), L"w")) {
            fputs(kDefaultIni, f);
            fclose(f);
            LOG("config: wrote default settings file");
        }
    }
    const wchar_t* ini = iniPath.c_str();
    Config c;
    c.udpPort         = readI(ini, "Network", "UdpPort", 4242);
    c.bindAddress     = readStr(ini, "Network", "BindAddress", "127.0.0.1");
    c.enableOnStartup = readI(ini, "General", "EnableOnStartup", 1) != 0;
    c.cockpitOnly     = readI(ini, "General", "CockpitOnly", 1) != 0;
    c.staleTimeoutMs  = readI(ini, "General", "StaleTimeoutMs", 500);
    c.smoothing       = readF(ini, "General", "Smoothing", 0.0);
    c.toggleKey        = readI(ini, "Hotkeys", "ToggleKey", VK_END);
    c.recenterKey      = readI(ini, "Hotkeys", "RecenterKey", VK_HOME);
    c.chordToggleKey   = readI(ini, "Hotkeys", "ChordToggleKey", 'Y');
    c.chordRecenterKey = readI(ini, "Hotkeys", "ChordRecenterKey", 'H');
    c.map.yaw   = readAxis(ini, "Rotation", "Yaw", 150.0f);
    c.map.pitch = readAxis(ini, "Rotation", "Pitch", 60.0f);
    c.map.roll  = readAxis(ini, "Rotation", "Roll", 30.0f);
    c.map.positionEnabled = readI(ini, "Position", "Enabled", 1) != 0;
    c.map.x = readAxis(ini, "Position", "X", 0.25f);
    c.map.y = readAxis(ini, "Position", "Y", 0.15f);
    c.map.z = readAxis(ini, "Position", "Z", 0.20f);
    readOrder(ini, "RotOrder", c.map.rotOrder);
    readOrder(ini, "PosOrder", c.map.posOrder);
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
