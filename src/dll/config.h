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
    int toggleKey = VK_END, recenterKey = VK_HOME;
    int chordToggleKey = 'Y', chordRecenterKey = 'H';   // used with Ctrl+Shift
    // Pose mapping
    MapCfg map;
};

// Loads the INI, writing a commented default file first if it does not exist.
Config loadConfig(const std::wstring& iniPath);

} // namespace fh2
