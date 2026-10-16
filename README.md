# FH2 Head Tracking for Azimuth

**Status: 0.1.0, first test build. Not yet verified in-game.**

Webcam head tracking for Forza Horizon 2 running in Azimuth. It drives the game's **own
head-tracking camera layers**, the ones built for Kinect, with your head pose from
OpenTrack. You get yaw, pitch and roll, plus lean left/right, up/down and forward/back,
in the cockpit view.

Unofficial. Not affiliated with Azimuth / Almost There Games, Microsoft or Playground Games.
The Azimuth developer does not support modified setups: **close FH2HeadTracking.exe before
reporting any Azimuth bug.**

Supported: Forza Horizon 2 **1.0.0.64** (with or without Storm Island) on Azimuth 0.9.x.
On any other game build the plugin detects the difference, changes nothing, and logs why.

## How it works

The game evaluates a head-tracking input for each camera layer every frame
(`HeadTrackingX/Y/Z/Yaw/Pitch` in CameraPhysics.xml). In this build the Kinect side of
those inputs is almost empty. X, Y and Z come from a function that always returns zero,
pitch is always zero, and yaw is only body lean, capped at about 12°.

The plugin redirects those five input reads. For the cockpit layers it supplies your
OpenTrack pose:

* rotation goes through the cockpit `HeadTrackingYaw → CameraSpaceYPROffset` layer
* position goes through the cockpit `HeadTrackingX → CarSpaceXYZOffset` layer

The game then applies them with its own layer maths and spring smoothing. Nothing is
written to the Azimuth or game folders. Camera.zip is not modified; the layer ranges
are widened in memory.

## Install

1. Install [OpenTrack](https://github.com/opentrack/opentrack/releases).
   * **Input:** `neuralnet tracker`. In its options, pick your webcam.
   * **Output:** `UDP over network`, address `127.0.0.1`, port `4242`.
   * **Filter:** `Accela` (default) is a good start.
   * Press **Start**. Bind a key to **Center** in OpenTrack's Options → Shortcuts.
2. Unzip this folder anywhere **outside** the Azimuth folder, e.g. `C:\Games\FH2HeadTracking`.
3. Start head tracking. Use either:
   * **A:** Double-click `FH2HeadTracking.exe`, then start Azimuth as usual. Leave the
     window open while you play.
   * **B:** Drag `azimuth.exe` onto `FH2HeadTracking.exe`. It starts Azimuth and closes itself
     when Azimuth exits. A shortcut works too:
     `FH2HeadTracking.exe "E:\XOne\Forza Horizon 2 Azimuth 0.9.10\azimuth-windows-0.9.10\azimuth.exe"`
4. Drive in the **cockpit camera**.

Windows Defender may flag FH2HeadTracking.exe, because it loads a DLL into another process,
which is what injectors do. If it does, allow it, or use the ASI loader option below.

**Alternative without the injector:** copy Ultimate ASI Loader's `dinput8.dll` next to
`azimuth.exe`, renamed to `winmm.dll`, and copy `FH2HeadTracking.dll` there renamed to
`FH2HeadTracking.asi`. This does put files in the Azimuth folder.

## Controls

| Action | Key | Chord |
|---|---|---|
| Toggle head tracking | `End` | `Ctrl+Shift+Y` |
| Recenter (in game) | `Home` | `Ctrl+Shift+H` |

Both are remappable in `FH2HeadTracking.ini`. That file is created next to the DLL on the
first run. Edit it, then restart the game.

## First test: what to check

`FH2HeadTracking.log` is written next to the DLL. A good run looks like this:

```
FH2 Head Tracking 0.1.0 loaded into pid ... (...\azimuth.exe)
udp: listening on 127.0.0.1:4242
waiting for the Forza Horizon 2 image to be mapped...
game image found at 0000...
patch: 5 head-tracking inputs redirected (thunks at ...)
udp: tracking data flowing
layer ... input 31 output 4 Param.x 12.000 -> rotation (cockpit)
layer ... input 28 output 2 Param.x 0.030 -> position (cockpit)
```

The `layer ...` lines only appear once you are driving in cockpit view.

Then calibrate one axis at a time (set the others' `Limit=0` if it helps):

1. **Yaw:** turn your head left. If the view turns right, set `YawInvert=1`.
2. **Pitch:** look up. If the view tilts sideways instead, swap the order:
   `RotOrder=0,2,1`. If it goes the wrong way, set `PitchInvert=1`.
3. **Roll:** tilt your head. Same fixes, with `RollInvert`.
4. **Position:** lean left/right, up/down and forward/back. Fix direction with
   `XInvert`/`YInvert`/`ZInvert`, and swap axes with `PosOrder=`.
5. Strength: most people want rotation larger than their actual head turn. Set it with
   OpenTrack's Mapping curves (preferred), or with `YawGain` etc.

Please send back the log and what each axis did. Reports from that first session
decide what changes in 0.2.

## Troubleshooting

* **No log file:** the DLL was not loaded. Make sure FH2HeadTracking.exe was running
  before or while Azimuth started, and that antivirus did not block it.
* **"this game build is not recognised":** you have a different FH2 version than
  1.0.0.64. Nothing was changed.
* **"bind ... failed":** another program is already using UDP port 4242 (e.g. a second
  tracker). Change `UdpPort` here and in OpenTrack.
* **Patched, but no `layer` lines in cockpit view:** the game is not evaluating its
  head-tracking layers in your setup. Please report it, with the log.
* **Jitter:** improve the lighting on your face, use OpenTrack's Accela filter, or set
  `Smoothing=0.2`.

## Uninstall

Close FH2HeadTracking.exe and delete its folder. Nothing else was changed.

## Building from source

```
src/dll/        plugin loaded into azimuth.exe (hook, tracker, config)
src/injector/   FH2HeadTracking.exe launcher
tests/          pose unit tests, integration harness, fake azimuth.exe
docs/           reverse-engineering notes
```

**Windows (Visual Studio 2022 or MSYS2 MinGW64):**
```
cmake -S . -B build -A x64
cmake --build build --config Release
```

**Linux cross-build (mingw-w64):** run `./build.sh`. It builds both binaries and the harness,
runs the pose unit tests, and packages `dist/`.

**Integration test.** `harness.exe` needs your own copy of the game executable; none is
included. It maps the real game executable, loads the DLL, sends OpenTrack packets and runs
the game's own `CameraLayer::EvaluateInput` against fake layers. Add `--deep` to exercise the
fallback memory scan. It runs on Windows or Wine:
```
harness.exe "<game>\Mount\forza_durango_release_final.exe" build\FH2HeadTracking.dll [--deep]
```

How the game was analysed, and every address and offset used, is documented in
[docs/REVERSE_ENGINEERING.md](docs/REVERSE_ENGINEERING.md) and `src/dll/game_layout.h`.
This repository contains no game code or data. Code is matched by hash and by an opcode skeleton.

## License

MIT, see [LICENSE](LICENSE). Forza Horizon 2 is a trademark of Microsoft. This project is
unofficial and not affiliated with Microsoft, Playground Games, Turn 10 or Almost There Games
(Azimuth).
