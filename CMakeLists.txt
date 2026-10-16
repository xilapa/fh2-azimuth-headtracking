cmake_minimum_required(VERSION 3.16)
project(fh2_azimuth_headtracking VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Build for x64: Azimuth is a 64-bit process.")
endif()

if(MSVC)
  add_compile_options(/W4 /utf-8)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type)
  add_link_options(-static -static-libgcc -static-libstdc++)
endif()

# The plugin loaded into azimuth.exe
add_library(FH2HeadTracking SHARED
  src/dll/dllmain.cpp
  src/dll/config.cpp
  src/dll/tracker.cpp
  src/dll/hook.cpp)
target_link_libraries(FH2HeadTracking PRIVATE ws2_32)
set_target_properties(FH2HeadTracking PROPERTIES PREFIX "")

# The launcher / injector
add_executable(FH2HeadTrackingLauncher src/injector/injector.cpp)
set_target_properties(FH2HeadTrackingLauncher PROPERTIES OUTPUT_NAME FH2HeadTracking)
target_link_libraries(FH2HeadTrackingLauncher PRIVATE psapi)
if(MSVC)
  target_link_options(FH2HeadTrackingLauncher PRIVATE /ENTRY:wmainCRTStartup)
else()
  target_link_options(FH2HeadTrackingLauncher PRIVATE -municode)
endif()

# Integration test (needs your own copy of the game executable to run)
add_executable(harness tests/harness.cpp)
target_link_libraries(harness PRIVATE ws2_32)
if(NOT MSVC)
  target_link_options(harness PRIVATE -municode)
endif()
