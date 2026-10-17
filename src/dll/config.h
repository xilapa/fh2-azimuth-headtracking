#pragma once
#include <windows.h>
#include <string>
#include "pose.h"

namespace fh2 {

enum class Source { Auto, Udp, FreeTrack };

struct Config {
    // [Input]
    Source source = Source::Auto;
    bool   diagnostics = true;
    // [Network]
    int          udpPort = 4242;
    std::string  bindAddress = "127.0.0.1";
    // [General]
    bool enableOnStartup = true;
    bool cockpitOnly     = true;     // only DriverCam layers; false = also hood/bumper
    int  staleTimeoutMs  = 500;
    double smoothing     = 0.0;      // 0..1
    // [Hotkeys] Windows virtual key codes
    int toggleKey = VK_END, recenterKey = 0;
    int chordToggleKey = 'Y', chordRecenterKey = 0;     // used with Ctrl+Shift; 0 = none
    unsigned recenterButtons = 0x00C0;                  // XInput mask, default L3+R3
    // Pose mapping
    MapCfg map;
};

// Loads the INI, writing a commented default file first if it does not exist.
Config loadConfig(const std::wstring& iniPath);

} // namespace fh2
