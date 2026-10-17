# FH2 Head Tracking for Azimuth

**Status: 0.3.0. Tested in-game with Beam Eye Tracker (FreeTrack input).**

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

1. Set up a head tracker. Either of these works. When both are running, FreeTrack is used:
   * **Beam Eye Tracker:** Game Connection → **Head Tracking Device Emulation**, with
     Driver set to `Compatibility`. Beam then publishes the FreeTrack/TrackIR shared memory,
     and the plugin reads it directly.
   * **[OpenTrack](https://github.com/opentrack/opentrack/releases)** with a webcam:
     * **Input:** `neuralnet tracker`.
     * **Output:** `freetrack 2.0 Enhanced`, or `UDP over network` (`127.0.0.1`, port `4242`).
     * **Filter:** `Accela`.
     * Bind a key to **Center**.
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

| Action | Default |
|---|---|
| Recenter | **L3 + R3** on an Xbox controller (both sticks clicked together) |
| Toggle head tracking | `End`, or `Ctrl+Shift+Y` |

Both clicks also reach the game, as usual. You can also recenter in your tracker
(Beam / OpenTrack). All of these are remappable in `FH2HeadTracking.ini`:
* `RecenterButtons`, e.g. `BACK+START`.
* `RecenterKey` / `ChordRecenterKey` to add a keyboard key. That file is created next to the DLL on the
first run. Edit it, then restart the game.

## Checking that it works

`FH2HeadTracking.log` is written next to the DLL. A good run looks like this:

```
FH2 Head Tracking 0.3.0 loaded into pid ... (...\azimuth.exe)
waiting for the Forza Horizon 2 image to be mapped...
game image found at 0000...
patch: 5 head-tracking inputs redirected (thunks at ...)
freetrack: shared memory found
input: using freetrack
layer ... input 31 output 4 Param.x 12.000 -> rotation (cockpit)
layer ... input 28 output 2 Param.x 0.030 -> position (cockpit)
```

The `layer ...` lines only appear once you are driving in cockpit view.

## Tuning

The defaults have been tested with Beam. Use these settings if your tracker behaves
differently:

* **Direction:** if an axis moves the wrong way, set its `Invert` to `1`: `YawInvert`,
  `PitchInvert`, `RollInvert`, `XInvert`, `YInvert`, `ZInvert`. To swap axes, use
  `RotOrder=` / `PosOrder=`.
* **Strength:** set the curves in your tracker (Beam's sensitivity, or OpenTrack's Mapping
  window), or use `YawGain`, `PitchGain` and so on here.
* **Range:** `Limit` caps the camera movement. The defaults keep the view inside the
  cabin. `Limit=0` turns an axis off.

## Troubleshooting

* **The view moves much more than your head, or keeps turning after you face forward
  (Beam):** Beam's "Immersive Camera" already amplifies head rotation and adds eye-gaze
  extension, so the view keeps panning while your eyes rest near the edge of the screen.
  Lower the camera sensitivity or turn off eye-based extension in Beam, or reduce
  `YawGain`/`PitchGain` here.
  Set `Diagnostics=1` and the log shows what arrives every 2 seconds
  (`diag freetrack: in yaw ...`). If `in yaw` keeps growing while your head is still, the
  extra movement comes from the tracker.

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

## Releases

Releases are built by GitHub Actions. Run **Actions → Release → Run workflow**. Leave
*version* empty to use the version from `CMakeLists.txt`. The workflow then:

1. builds the plugin and launcher with mingw-w64,
2. runs the unit tests,
3. packages `FH2HeadTracking-<version>.zip`,
4. creates the `v<version>` tag and a GitHub release.

## License

Public domain, under [The Unlicense](LICENSE). Anyone may use, copy, modify and
distribute it for any purpose, with no conditions.

Author: Dirceu da Silva Junior (see [AUTHORS](AUTHORS)).

Forza Horizon 2 is a trademark of Microsoft. This project is unofficial and not affiliated
with Microsoft, Playground Games, Turn 10 or Almost There Games (Azimuth).
