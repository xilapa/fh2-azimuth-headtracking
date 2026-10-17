#include "config_file.h"
#include "log.h"
#include <cstdio>

namespace fh2 {

static const char kDefaultIni[] =
"; FH2 Head Tracking for Azimuth - settings. Edit, then restart the game.\n"
"\n"
"[Input]\n"
"ConfigVersion=3\n"
"; auto      = FreeTrack/TrackIR shared memory when present (Beam \"Head Tracking Device\n"
";             Emulation\", OpenTrack \"freetrack 2.0 Enhanced\"), otherwise UDP\n"
"; freetrack = shared memory only\n"
"; udp       = OpenTrack \"UDP over network\" only\n"
"Source=auto\n"
"; 1 = write the received pose and the camera offsets to the log every 2 seconds (troubleshooting)\n"
"Diagnostics=0\n"
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
"; Recenter is on the controller by default (see RecenterButtons). 0 = no key.\n"
"RecenterKey=0\n"
"; Same actions with Ctrl+Shift+<key>, for keyboards without a nav cluster.\n"
"ChordToggleKey=0x59\n"
"ChordRecenterKey=0\n"
"; Xbox controller buttons that recenter when pressed together, joined with +.\n"
"; A B X Y LB RB L3 R3 BACK START UP DOWN LEFT RIGHT. Empty = off.\n"
"RecenterButtons=L3+R3\n"
"\n"
"; Gain multiplies your head movement. Invert flips an axis. Limit caps the\n"
"; camera offset (degrees for rotation, metres for position); Limit=0 turns\n"
"; that axis off. Sensitivity curves belong in OpenTrack's Mapping window.\n"
"[Rotation]\n"
"YawGain=1.0\n"
"YawInvert=0\n"
"YawLimit=90\n"
"PitchGain=1.0\n"
"PitchInvert=0\n"
"PitchLimit=40\n"
"RollGain=1.0\n"
"RollInvert=0\n"
"RollLimit=20\n"
"\n"
"[Position]\n"
"Enabled=1\n"
"XGain=1.0\n"
"XInvert=0\n"
"XLimit=0.10\n"
"YGain=1.0\n"
"YInvert=0\n"
"YLimit=0.06\n"
"ZGain=1.0\n"
"ZInvert=0\n"
"ZLimit=0.08\n"
"\n"
"[Advanced]\n"
"; Which component of the game's rotation Param receives yaw, pitch, roll (0,1,2).\n"
"RotOrder=0,1,2\n"
"; Which car-space component receives head X, Y, Z (0,1,2).\n"
"PosOrder=0,1,2\n";

constexpr int kConfigVersion = 3;

void ensureConfigFile(const std::wstring& iniPath) {
    if (GetFileAttributesW(iniPath.c_str()) != INVALID_FILE_ATTRIBUTES &&
        GetPrivateProfileIntW(L"Input", L"ConfigVersion", 0, iniPath.c_str()) < kConfigVersion) {
        // Settings from an older version: keep a backup, start from the new defaults.
        std::wstring bak = iniPath + L".old.bak";
        MoveFileExW(iniPath.c_str(), bak.c_str(), MOVEFILE_REPLACE_EXISTING);
        LOG("config: settings from an older version saved as FH2HeadTracking.ini.old.bak, writing new defaults");
    }
    if (GetFileAttributesW(iniPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (FILE* f = _wfopen(iniPath.c_str(), L"w")) {
            fputs(kDefaultIni, f);
            fclose(f);
            LOG("config: wrote default settings file");
        }
    }
}

} // namespace fh2
