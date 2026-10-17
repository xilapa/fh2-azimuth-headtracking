#!/usr/bin/env bash
# Cross-builds the plugin and launcher with mingw-w64, runs the unit tests and
# packages dist/FH2HeadTracking-<version>.zip.
# Version: $VERSION if set, otherwise project(VERSION) from CMakeLists.txt.
set -euo pipefail
cd "$(dirname "$0")"
VERSION="${VERSION:-$(sed -nE 's/^project\([^)]*VERSION ([0-9][0-9.]*).*/\1/p' CMakeLists.txt)}"
[ -n "$VERSION" ] || { echo "could not determine version" >&2; exit 1; }
CXX=x86_64-w64-mingw32-g++
FLAGS="-std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -static -static-libgcc -static-libstdc++ -s"
PKG=dist/FH2HeadTracking

rm -rf build dist && mkdir -p build "$PKG"
g++ -std=c++17 -O1 -Wall tests/test_pose.cpp -o build/test_pose && ./build/test_pose

$CXX $FLAGS -DFH2HT_VERSION="\"$VERSION\"" -shared -o build/FH2HeadTracking.dll \
    src/dll/dllmain.cpp src/dll/config.cpp src/dll/config_file.cpp src/dll/tracker.cpp src/dll/hook.cpp \
    -lws2_32 -Wl,--subsystem,windows
$CXX $FLAGS -municode -o build/FH2HeadTracking.exe src/injector/injector.cpp src/dll/config.cpp src/dll/config_file.cpp -lpsapi
# integration harness (needs your own copy of the game executable to run)
$CXX $FLAGS -municode -mavx -o build/harness.exe tests/harness.cpp -lws2_32

cp build/FH2HeadTracking.dll build/FH2HeadTracking.exe "$PKG"/
cp README.md "$PKG"/README.md
(cd dist && zip -qr "FH2HeadTracking-$VERSION.zip" FH2HeadTracking)
echo "built dist/FH2HeadTracking-$VERSION.zip"
