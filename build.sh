#!/usr/bin/env bash
# Cross-builds the plugin and injector with mingw-w64, runs host tests, packages a zip.
set -euo pipefail
cd "$(dirname "$0")"
CXX=x86_64-w64-mingw32-g++
FLAGS="-std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -static -static-libgcc -static-libstdc++ -s"

mkdir -p build dist/FH2HeadTracking
g++ -std=c++17 -O1 -Wall tests/test_pose.cpp -o build/test_pose && ./build/test_pose

$CXX $FLAGS -shared -o build/FH2HeadTracking.dll \
    src/dll/dllmain.cpp src/dll/config.cpp src/dll/tracker.cpp src/dll/hook.cpp \
    -lws2_32 -Wl,--subsystem,windows

$CXX $FLAGS -municode -o build/FH2HeadTracking.exe src/injector/injector.cpp -lpsapi
$CXX $FLAGS -municode -mavx -o build/harness.exe tests/harness.cpp -lws2_32

cp build/FH2HeadTracking.dll build/FH2HeadTracking.exe dist/FH2HeadTracking/
cp README.md dist/FH2HeadTracking/README.md
cp LICENSE dist/FH2HeadTracking/LICENSE.txt
(cd dist && rm -f FH2HeadTracking-0.2.0.zip && zip -qr FH2HeadTracking-0.2.0.zip FH2HeadTracking)
echo "built dist/FH2HeadTracking-0.2.0.zip"
