// Integration test (runs under Windows or Wine):
//   1. maps the real forza_durango_release_final.exe into memory the way a loader would,
//   2. loads FH2HeadTracking.dll, which must find, verify and patch the image,
//   3. sends OpenTrack UDP packets,
//   4. calls the game's own CameraLayer::EvaluateInput on fake layers and checks results.
// usage: harness.exe <path-to-game-exe> <path-to-FH2HeadTracking.dll> [--deep]
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

#ifdef _MSC_VER
#define MSABI
#else
#define MSABI __attribute__((ms_abi))
#endif

static int failures = 0;
#define CHECK(cond, ...) do { if (cond) { printf("  ok   "); } else { printf("  FAIL "); ++failures; } printf(__VA_ARGS__); printf("\n"); } while (0)

static uint8_t* mapImage(const wchar_t* path, size_t pad = 0) {
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return nullptr;
    DWORD size = GetFileSize(f, nullptr);
    std::vector<uint8_t> file(size);
    DWORD got; ReadFile(f, file.data(), size, &got, nullptr); CloseHandle(f);
    auto dos = (IMAGE_DOS_HEADER*)file.data();
    auto nt = (IMAGE_NT_HEADERS64*)(file.data() + dos->e_lfanew);
    // pad > 0 places the image inside a larger allocation, which defeats the plugin's
    // fast path and exercises its deep memory scan.
    uint8_t* alloc = (uint8_t*)VirtualAlloc(nullptr, nt->OptionalHeader.SizeOfImage + pad, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!alloc) return nullptr;
    uint8_t* base = alloc + pad;
    memcpy(base, file.data(), nt->OptionalHeader.SizeOfHeaders);
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        if (sec[i].SizeOfRawData)
            memcpy(base + sec[i].VirtualAddress, file.data() + sec[i].PointerToRawData,
                   std::min(sec[i].SizeOfRawData, sec[i].Misc.VirtualSize ? sec[i].Misc.VirtualSize : sec[i].SizeOfRawData));
    // relocations
    uint64_t delta = (uint64_t)base - nt->OptionalHeader.ImageBase;
    auto& rd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    uint8_t* r = base + rd.VirtualAddress; uint8_t* rend = r + rd.Size;
    while (r < rend) {
        auto blk = (IMAGE_BASE_RELOCATION*)r;
        if (!blk->SizeOfBlock) break;
        auto ent = (WORD*)(blk + 1);
        int n = (blk->SizeOfBlock - sizeof(*blk)) / 2;
        for (int i = 0; i < n; ++i)
            if ((ent[i] >> 12) == IMAGE_REL_BASED_DIR64)
                *(uint64_t*)(base + blk->VirtualAddress + (ent[i] & 0xFFF)) += delta;
        r += blk->SizeOfBlock;
    }
    return base;
}

// Fake car object: EvaluateInput calls car->vtbl[0x70/8]() twice and needs non-null.
struct FakeCar;
static void* MSABI carSlot14(FakeCar* self) { return self; }
static void* g_carVtbl[32];
struct FakeCar { void** vtbl; uint8_t pad[0x400]; };

typedef void (MSABI *EvalFn)(void* layer, void* car, void* r8, float dt);

struct alignas(16) Layer { uint8_t b[0x200]; };

static Layer makeLayer(int inputType, int outputType, float paramX) {
    Layer L; memset(&L, 0, sizeof(L));
    *(int*)(L.b + 0x48) = inputType;
    *(int*)(L.b + 0x50) = outputType;
    *(float*)(L.b + 0x58) = -25.0f;
    *(float*)(L.b + 0x5C) = 25.0f;
    *(int*)(L.b + 0xAC) = 4;          // FivePoint
    *(float*)(L.b + 0xE0) = paramX;
    return L;
}
static float f(const Layer& L, int off) { return *(const float*)(L.b + off); }
static int   i32(const Layer& L, int off) { return *(const int*)(L.b + off); }

static void sendPose(SOCKET s, double x, double y, double z, double yaw, double pitch, double roll) {
    double p[6] = {x, y, z, yaw, pitch, roll};
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(4242);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    sendto(s, (const char*)p, sizeof(p), 0, (sockaddr*)&a, sizeof(a));
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 3) { printf("usage: harness <game.exe> <FH2HeadTracking.dll> [--deep]\n"); return 2; }
    bool deep = argc > 3 && wcscmp(argv[3], L"--deep") == 0;
    uint8_t* base = mapImage(argv[1], deep ? 0x230000 : 0);
    if (!base) { printf("could not map game image\n"); return 2; }
    printf("game image mapped at %p\n", base);
    uint8_t original[5]; memcpy(original, base + 0x103832A, 5);

    if (!LoadLibraryW(argv[2])) { printf("LoadLibrary failed %lu\n", GetLastError()); return 2; }

    WSADATA w; WSAStartup(MAKEWORD(2, 2), &w);
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // wait for the patch
    bool patched = false;
    for (int i = 0; i < 150 && !patched; ++i) { Sleep(100); patched = memcmp(base + 0x103832A, original, 5) != 0; }
    CHECK(patched, "plugin patched the yaw input call site");
    CHECK(base[0x103832A] == 0xE8, "call opcode preserved");

    // First packet sets the (identity) centre, then a real pose.
    sendPose(s, 0, 0, 0, 0, 0, 0); Sleep(50);
    sendPose(s, 4.0, 2.0, -3.0, 30.0, -10.0, 5.0); Sleep(150);

    g_carVtbl[0x70 / 8] = (void*)&carSlot14;
    FakeCar car; car.vtbl = g_carVtbl;
    EvalFn eval = (EvalFn)(base + 0x1037DF0);

    printf("cockpit yaw layer (HeadTrackingYaw -> CameraSpaceYPROffset, Param.x=12):\n");
    Layer rot = makeLayer(31, 4, 12.0f);
    eval(&rot, &car, nullptr, 0.016f);
    CHECK(std::fabs(f(rot, 0) - 1.0f) < 1e-4f, "input forced to 1.0 (got %f)", f(rot, 0));
    CHECK(std::fabs(f(rot, 0xE0) - 30.0f) < 1e-3f && std::fabs(f(rot, 0xE4) + 10.0f) < 1e-3f && std::fabs(f(rot, 0xE8) - 5.0f) < 1e-3f,
          "Param = yaw/pitch/roll (%f %f %f)", f(rot, 0xE0), f(rot, 0xE4), f(rot, 0xE8));
    CHECK(i32(rot, 0xAC) == 1, "mapping set to Linear");
    CHECK(f(rot, 0x58) == -2.0f && f(rot, 0x5C) == 2.0f, "clamp widened");

    printf("cockpit X layer (HeadTrackingX -> CarSpaceXYZOffset, Param.x=0.03):\n");
    Layer pos = makeLayer(28, 2, 0.03f);
    eval(&pos, &car, nullptr, 0.016f);
    CHECK(std::fabs(f(pos, 0) - 1.0f) < 1e-6f, "input forced to 1.0 (got %f)", f(pos, 0));
    CHECK(std::fabs(f(pos, 0xE0) - 0.04f) < 1e-5f && std::fabs(f(pos, 0xE4) - 0.02f) < 1e-5f && std::fabs(f(pos, 0xE8) + 0.03f) < 1e-5f,
          "Param = x/y/z metres (%f %f %f)", f(pos, 0xE0), f(pos, 0xE4), f(pos, 0xE8));

    printf("layers that must stay neutral:\n");
    Layer y = makeLayer(29, 2, 0.0f);
    *(float*)(y.b + 0xE4) = 1.0f;
    eval(&y, &car, nullptr, 0.016f);
    CHECK(f(y, 0) == 0.0f && f(y, 0xE4) == 1.0f, "HeadTrackingY layer gets 0 and is untouched");
    Layer pitch = makeLayer(32, 4, 5.0f);
    eval(&pitch, &car, nullptr, 0.016f);
    CHECK(f(pitch, 0) == 0.0f, "HeadTrackingPitch layer gets 0");
    Layer hood = makeLayer(31, 4, 15.0f);
    eval(&hood, &car, nullptr, 0.016f);
    CHECK(f(hood, 0) == 0.0f && f(hood, 0xE0) == 15.0f, "hood/bumper yaw layer untouched with CockpitOnly=1");
    Layer rot2 = makeLayer(31, 4, 12.0f);
    eval(&rot, &car, nullptr, 0.016f);
    eval(&rot2, &car, nullptr, 0.016f);
    CHECK(f(rot, 0) == 1.0f && f(rot2, 0) == 0.0f, "second cockpit rotation layer in the same frame is neutral");
    Sleep(20);

    printf("updates and staleness:\n");
    sendPose(s, 0, 0, 0, -45.0, 0, 0); Sleep(100);
    eval(&rot, &car, nullptr, 0.016f);
    CHECK(std::fabs(f(rot, 0xE0) + 45.0f) < 1e-3f, "new pose arrives (yaw %f)", f(rot, 0xE0));
    Sleep(900);   // > StaleTimeoutMs
    eval(&rot, &car, nullptr, 0.016f);
    CHECK(f(rot, 0) == 0.0f, "stale tracker -> input 0, camera returns to rest");

    printf("FreeTrack shared memory (Beam / TrackIR emulation):\n");
    HANDLE map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 4096, L"FT_SharedMem");
    float* ft = (float*)MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, 4096);
    uint32_t* id = (uint32_t*)ft;
    // layout: DataID, CamW, CamH, Yaw, Pitch, Roll, X, Y, Z (radians, mm)
    for (int i = 0; i < 75; ++i) {
        ft[3] = -0.5f; ft[4] = 0.2f; ft[5] = 0.1f; ft[6] = 50.0f; ft[7] = -20.0f; ft[8] = 30.0f;
        *id += 1;
        sendPose(s, 0, 0, 0, 5.0, 0, 0);     // UDP keeps sending; FreeTrack must win
        Sleep(20);
    }
    eval(&rot, &car, nullptr, 0.016f);
    eval(&pos, &car, nullptr, 0.016f);
    CHECK(f(rot, 0) == 1.0f && std::fabs(f(rot, 0xE0) - 28.6479f) < 0.01f, "FreeTrack yaw -0.5 rad -> +28.65 deg (got %f)", f(rot, 0xE0));
    CHECK(std::fabs(f(rot, 0xE4) + 11.4592f) < 0.01f && std::fabs(f(rot, 0xE8) - 5.7296f) < 0.01f,
          "FreeTrack pitch/roll converted (%f %f)", f(rot, 0xE4), f(rot, 0xE8));
    CHECK(std::fabs(f(pos, 0xE0) - 0.05f) < 1e-4f && std::fabs(f(pos, 0xE4) + 0.02f) < 1e-4f && std::fabs(f(pos, 0xE8) - 0.03f) < 1e-4f,
          "FreeTrack mm -> metres (%f %f %f)", f(pos, 0xE0), f(pos, 0xE4), f(pos, 0xE8));

    printf(failures ? "\n%d check(s) FAILED\n" : "\nall integration checks passed\n", failures);
    return failures ? 1 : 0;
}
