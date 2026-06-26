# Godfather Discord Rich Presence — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a native `.asi` plugin (x86 DLL) that reads *The Godfather* (PC, 2006) game state from memory and publishes a Discord Rich Presence showing the player's scene (menu/loading/in-game) and current district.

**Architecture:** A DLL renamed to `.asi`, auto-loaded by the Ultimate ASI Loader already present as `dinput8.dll`. It runs inside `godfather.exe`, spawns one background thread that polls game memory (offsets read from an editable `.ini`, relative to the module base, with a degraded mode when offsets are missing) and pushes activity updates to Discord over the native IPC named pipe. Four single-responsibility units: `Config`, `GameState`, `DiscordIPC`, and `dllmain`.

**Tech Stack:** C++17, MSVC (cl.exe), CMake, Win32 API (named pipes, `GetModuleHandle`, SEH). No external libraries. Target architecture: **x86 (32-bit)**.

## Global Constraints

- **Target architecture: x86 (32-bit)** — the game is a 32-bit process; the DLL MUST be 32-bit or it will not load.
- **Toolchain: MSVC + CMake.** Generator must target Win32 (`-A Win32`).
- **No external dependencies.** No discord-rpc, no JSON library, no vcpkg packages. Win32 + C++ standard library only.
- **The plugin must never crash or hang the game.** All memory reads guarded by SEH (`__try/__except`). The background thread must never throw across the DLL boundary. `DllMain` must never block (no I/O, no thread joins inside the loader lock).
- **Discord Client ID:** `1520195420398289017` (default in `.ini`, overridable).
- **Output artifact name:** `GodfatherDiscordRPC.asi` (a DLL with `.asi` extension), deployed to the game's `scripts/` folder.
- **Language:** code comments and identifiers in English; user-facing RPC strings in Spanish (e.g. "En Hell's Kitchen", "En el menú principal", "Cargando...").
- **Offsets are relative to the `godfather.exe` module base**, never absolute addresses.

---

## File Structure

| File | Responsibility |
|---|---|
| `CMakeLists.txt` | Build config: x86 DLL target + two test executables |
| `src/Config.h` / `src/Config.cpp` | Parse `GodfatherDiscordRPC.ini`: client id, poll interval, memory offsets, district table |
| `src/GameState.h` / `src/GameState.cpp` | Read game memory (SEH-guarded), translate to `Scene` + districtId + display string |
| `src/DiscordIPC.h` / `src/DiscordIPC.cpp` | Named-pipe connect, handshake, SET_ACTIVITY, reconnect; manual JSON build + string escape |
| `src/Logger.h` / `src/Logger.cpp` | Append-only log to `GodfatherDiscordRPC.log` next to the DLL |
| `src/dllmain.cpp` | `.asi` entry point; spawn/stop background thread; orchestrate the poll loop |
| `dist/GodfatherDiscordRPC.ini` | Shipped config (client id + 9 districts pre-filled, offsets blank) |
| `test/unit_tests.cpp` | Unit tests for `Config` and `GameState` (simulated memory) |
| `test/ipc_host_test.cpp` | Standalone host exe: sends a fixed activity to Discord without the game |
| `docs/OFFSETS.md` | Cheat Engine guide for finding `scene_offset` and `district_offset` |

Build order of tasks mirrors dependency order: Logger → Config → DiscordIPC → GameState → dllmain → packaging/docs.

---

### Task 1: CMake build skeleton + Logger

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/Logger.h`, `src/Logger.cpp`
- Create: `test/unit_tests.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `namespace logger { void init(const std::wstring& dllDir); void write(const char* level, const std::string& msg); }`
  - `init` sets the log file path to `<dllDir>\GodfatherDiscordRPC.log`. `write` appends one line `[<level>] <msg>\n`. Both are no-throw.

- [ ] **Step 1: Write the failing test**

Create `test/unit_tests.cpp`:
```cpp
#include <cassert>
#include <cstdio>
#include <string>
#include <fstream>
#include "../src/Logger.h"

static int g_failures = 0;
#define CHECK(cond) do { if(!(cond)) { std::printf("FAIL: %s (line %d)\n", #cond, __LINE__); ++g_failures; } } while(0)

static void test_logger_writes_line() {
    logger::init(L".");
    logger::write("INFO", "hello world");
    std::ifstream f("GodfatherDiscordRPC.log");
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(content.find("[INFO] hello world") != std::string::npos);
}

int main() {
    test_logger_writes_line();
    if (g_failures == 0) std::printf("ALL TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
```

Create `CMakeLists.txt` (lists all final sources; note that `dllmain.cpp`, `GameState.cpp`, `DiscordIPC.cpp` are filled in later tasks — in this task you only build the `unit_tests` target, **not** the `.asi`, so the unwritten bodies don't matter yet):
```cmake
cmake_minimum_required(VERSION 3.20)
project(GodfatherDiscordRPC CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- The .asi plugin (a DLL) ---
add_library(GodfatherDiscordRPC SHARED
    src/dllmain.cpp
    src/Config.cpp
    src/GameState.cpp
    src/DiscordIPC.cpp
    src/Logger.cpp
)
# Output a .asi instead of .dll
set_target_properties(GodfatherDiscordRPC PROPERTIES SUFFIX ".asi" PREFIX "")

# --- Unit tests (console exe, no game needed) ---
add_executable(unit_tests
    test/unit_tests.cpp
    src/Config.cpp
    src/GameState.cpp
    src/Logger.cpp
)

# --- IPC host test (console exe, talks to Discord without the game) ---
add_executable(ipc_host_test
    test/ipc_host_test.cpp
    src/DiscordIPC.cpp
    src/Logger.cpp
)
```

- [ ] **Step 2: Stub the not-yet-written sources so unit_tests links**

Create empty-but-valid stubs so the `unit_tests` target compiles (full impls come in later tasks). Create `src/Config.h`:
```cpp
#pragma once
// Filled in Task 2.
```
Create `src/Config.cpp`:
```cpp
#include "Config.h"
// Filled in Task 2.
```
Create `src/GameState.h`:
```cpp
#pragma once
// Filled in Task 4.
```
Create `src/GameState.cpp`:
```cpp
#include "GameState.h"
// Filled in Task 4.
```

- [ ] **Step 3: Write the Logger header**

Create `src/Logger.h`:
```cpp
#pragma once
#include <string>

namespace logger {
    void init(const std::wstring& dllDir);
    void write(const char* level, const std::string& msg);
}
```

- [ ] **Step 4: Run test to verify it fails**

```powershell
cmake -S . -B build -A Win32
cmake --build build --config Debug --target unit_tests
.\build\Debug\unit_tests.exe
```
Expected: link error (unresolved `logger::init`/`logger::write`) OR test FAIL. Either confirms the test is real.

- [ ] **Step 5: Implement the Logger**

Create `src/Logger.cpp`:
```cpp
#include "Logger.h"
#include <fstream>
#include <mutex>

namespace {
    std::wstring g_path = L"GodfatherDiscordRPC.log";
    std::mutex   g_mtx;
}

namespace logger {
    void init(const std::wstring& dllDir) {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_path = dllDir + L"\\GodfatherDiscordRPC.log";
    }
    void write(const char* level, const std::string& msg) {
        try {
            std::lock_guard<std::mutex> lk(g_mtx);
            std::ofstream f(g_path, std::ios::app);
            if (f) f << "[" << level << "] " << msg << "\n";
        } catch (...) { /* never throw */ }
    }
}
```

- [ ] **Step 6: Run test to verify it passes**

```powershell
cmake --build build --config Debug --target unit_tests
.\build\Debug\unit_tests.exe
```
Expected: `ALL TESTS PASSED`, exit code 0.

- [ ] **Step 7: Commit**

```powershell
git add CMakeLists.txt src/Logger.h src/Logger.cpp src/Config.h src/Config.cpp src/GameState.h src/GameState.cpp test/unit_tests.cpp
git commit -m "feat: CMake x86 skeleton + Logger with passing test"
```

---

### Task 2: Config (.ini parser)

**Files:**
- Modify: `src/Config.h` (replace stub)
- Modify: `src/Config.cpp` (replace stub)
- Modify: `test/unit_tests.cpp` (add cases + call from `main`)

**Interfaces:**
- Consumes: `logger`.
- Produces:
  ```cpp
  struct Config {
      std::string clientId = "1520195420398289017";
      int pollIntervalMs = 2000;
      // Offsets relative to module base. -1 means "not configured" (degraded mode).
      long sceneOffset = -1;
      long districtOffset = -1;
      std::map<int, std::string> districts; // id -> display name
  };
  // Parses the ini at the given path. Missing file / missing keys => defaults
  // (including the 9 built-in districts). Never throws.
  Config loadConfig(const std::wstring& iniPath);
  ```
  Built-in default districts (used when the ini omits a `[Districts]` section): the same 9 listed in the spec, ids 0..8.

- [ ] **Step 1: Write the failing tests**

Append to `test/unit_tests.cpp` (before `main`):
```cpp
#include "../src/Config.h"

static void writeFile(const char* path, const std::string& content) {
    std::ofstream f(path); f << content;
}

static void test_config_defaults_when_missing() {
    Config c = loadConfig(L"does_not_exist.ini");
    CHECK(c.clientId == "1520195420398289017");
    CHECK(c.pollIntervalMs == 2000);
    CHECK(c.sceneOffset == -1);
    CHECK(c.districtOffset == -1);
    CHECK(c.districts.size() == 9);
    CHECK(c.districts[0] == "Hell's Kitchen");
    CHECK(c.districts[8] == "Woltz Compound");
}

static void test_config_parses_values() {
    writeFile("t2.ini",
        "[Discord]\nclient_id = 123\n"
        "[Settings]\npoll_interval_ms = 500\n"
        "[Memory]\nscene_offset = 0x10\ndistrict_offset = 4096\n"
        "[Districts]\n0 = Casa\n1 = Calle\n");
    Config c = loadConfig(L"t2.ini");
    CHECK(c.clientId == "123");
    CHECK(c.pollIntervalMs == 500);
    CHECK(c.sceneOffset == 16);      // 0x10 hex parsed
    CHECK(c.districtOffset == 4096); // decimal parsed
    CHECK(c.districts.size() == 2);
    CHECK(c.districts[1] == "Calle");
}

static void test_config_blank_offsets_stay_minus_one() {
    writeFile("t3.ini", "[Memory]\nscene_offset =\ndistrict_offset =\n");
    Config c = loadConfig(L"t3.ini");
    CHECK(c.sceneOffset == -1);
    CHECK(c.districtOffset == -1);
}
```

Add these calls inside `main()` before the pass/fail print:
```cpp
    test_config_defaults_when_missing();
    test_config_parses_values();
    test_config_blank_offsets_stay_minus_one();
```

- [ ] **Step 2: Run test to verify it fails**

```powershell
cmake --build build --config Debug --target unit_tests
.\build\Debug\unit_tests.exe
```
Expected: compile error (Config undefined) or FAIL lines. Confirms tests are real.

- [ ] **Step 3: Write the Config header**

Replace `src/Config.h`:
```cpp
#pragma once
#include <string>
#include <map>

struct Config {
    std::string clientId = "1520195420398289017";
    int pollIntervalMs = 2000;
    long sceneOffset = -1;
    long districtOffset = -1;
    std::map<int, std::string> districts;
};

Config loadConfig(const std::wstring& iniPath);
```

- [ ] **Step 4: Implement Config**

Replace `src/Config.cpp`:
```cpp
#include "Config.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace {
    std::string trim(std::string s) {
        auto notSpace = [](int ch){ return !std::isspace(ch); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
        s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
        return s;
    }
    // Returns true and fills out if value is a valid int (decimal or 0x hex).
    bool parseLong(const std::string& v, long& out) {
        if (v.empty()) return false;
        try {
            size_t idx = 0;
            long val = std::stol(v, &idx, 0); // base 0 => auto-detect 0x
            if (idx != v.size()) return false;
            out = val;
            return true;
        } catch (...) { return false; }
    }
    void fillDefaultDistricts(std::map<int, std::string>& d) {
        d = {
            {0, "Hell's Kitchen"}, {1, "Midtown"}, {2, "SoHo"},
            {3, "Brooklyn"}, {4, "New Jersey"}, {5, "Holland Tunnel"},
            {6, "Lincoln Tunnel"}, {7, "Westside Parkway"}, {8, "Woltz Compound"}
        };
    }
}

Config loadConfig(const std::wstring& iniPath) {
    Config c;
    std::ifstream f(iniPath);
    if (!f) {
        logger::write("WARN", "ini not found, using defaults");
        fillDefaultDistricts(c.districts);
        return c;
    }

    std::string section, line;
    bool sawDistricts = false;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            if (section == "Districts") sawDistricts = true;
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (section == "Discord" && key == "client_id" && !val.empty()) {
            c.clientId = val;
        } else if (section == "Settings" && key == "poll_interval_ms") {
            long v; if (parseLong(val, v) && v > 0) c.pollIntervalMs = (int)v;
        } else if (section == "Memory" && key == "scene_offset") {
            long v; if (parseLong(val, v)) c.sceneOffset = v;
        } else if (section == "Memory" && key == "district_offset") {
            long v; if (parseLong(val, v)) c.districtOffset = v;
        } else if (section == "Districts") {
            long id; if (parseLong(key, id) && !val.empty()) c.districts[(int)id] = val;
        }
    }
    if (!sawDistricts || c.districts.empty()) fillDefaultDistricts(c.districts);
    return c;
}
```

- [ ] **Step 5: Run test to verify it passes**

```powershell
cmake --build build --config Debug --target unit_tests
.\build\Debug\unit_tests.exe
```
Expected: `ALL TESTS PASSED`.

- [ ] **Step 6: Commit**

```powershell
git add src/Config.h src/Config.cpp test/unit_tests.cpp
git commit -m "feat: ini config parser with defaults and degraded-mode offsets"
```

---

### Task 3: DiscordIPC (named pipe + handshake + activity)

**Files:**
- Create: `src/DiscordIPC.h`, `src/DiscordIPC.cpp`
- Create: `test/ipc_host_test.cpp`

**Interfaces:**
- Consumes: `logger`.
- Produces:
  ```cpp
  class DiscordIPC {
  public:
      explicit DiscordIPC(std::string clientId);
      ~DiscordIPC();
      bool isConnected() const;
      // Opens pipe (-0..-9) and performs handshake. Idempotent if already connected.
      bool connect();
      void disconnect();
      // Sends SET_ACTIVITY with the given state string + a fixed session start
      // timestamp. Returns false on pipe failure (caller should disconnect/retry).
      bool setActivity(const std::string& state, long long startUnix);
  };
  // Exposed free function for unit testing the escape logic:
  std::string jsonEscape(const std::string& s);
  ```

- [ ] **Step 1: Write the failing unit test for jsonEscape**

Append to `test/unit_tests.cpp` (before `main`):
```cpp
#include "../src/DiscordIPC.h"

static void test_json_escape() {
    CHECK(jsonEscape("Hell's Kitchen") == "Hell's Kitchen"); // apostrophe is legal raw in JSON
    CHECK(jsonEscape("a\"b") == "a\\\"b");
    CHECK(jsonEscape("a\\b") == "a\\\\b");
    CHECK(jsonEscape(std::string("a\nb")) == "a\\nb");
}
```
Add to `main()`:
```cpp
    test_json_escape();
```

- [ ] **Step 2: Write the DiscordIPC header**

Create `src/DiscordIPC.h`:
```cpp
#pragma once
#include <string>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

std::string jsonEscape(const std::string& s);

class DiscordIPC {
public:
    explicit DiscordIPC(std::string clientId);
    ~DiscordIPC();

    bool isConnected() const { return pipe_ != INVALID_HANDLE_VALUE; }
    bool connect();
    void disconnect();
    bool setActivity(const std::string& state, long long startUnix);

private:
    bool writeFrame(int opcode, const std::string& payload);
    bool readFrame(std::string& outPayload); // blocking-ish read of one frame

    std::string clientId_;
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
    long long nonce_ = 0;
};
```

- [ ] **Step 3: Run test to verify it fails**

```powershell
cmake --build build --config Debug --target unit_tests
.\build\Debug\unit_tests.exe
```
Expected: link error (`jsonEscape` unresolved) — unit_tests does not yet compile DiscordIPC.cpp.

- [ ] **Step 4: Add DiscordIPC.cpp to the unit_tests target**

In `CMakeLists.txt`, add `src/DiscordIPC.cpp` to the `unit_tests` executable source list so `jsonEscape` links:
```cmake
add_executable(unit_tests
    test/unit_tests.cpp
    src/Config.cpp
    src/GameState.cpp
    src/DiscordIPC.cpp
    src/Logger.cpp
)
```

- [ ] **Step 5: Implement DiscordIPC**

Create `src/DiscordIPC.cpp`:
```cpp
#include "DiscordIPC.h"
#include "Logger.h"
#include <cstdint>
#include <vector>

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
                else out += (char)c;
        }
    }
    return out;
}

DiscordIPC::DiscordIPC(std::string clientId) : clientId_(std::move(clientId)) {}
DiscordIPC::~DiscordIPC() { disconnect(); }

bool DiscordIPC::connect() {
    if (isConnected()) return true;
    for (int i = 0; i <= 9; ++i) {
        char path[64];
        std::snprintf(path, sizeof(path), "\\\\.\\pipe\\discord-ipc-%d", i);
        HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                               OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            pipe_ = h;
            std::string handshake = "{\"v\":1,\"client_id\":\"" + clientId_ + "\"}";
            if (!writeFrame(0 /*HANDSHAKE*/, handshake)) { disconnect(); continue; }
            std::string resp;
            if (!readFrame(resp)) { disconnect(); continue; }
            logger::write("INFO", "Discord IPC connected on pipe " + std::to_string(i));
            return true;
        }
    }
    return false;
}

void DiscordIPC::disconnect() {
    if (pipe_ != INVALID_HANDLE_VALUE) { CloseHandle(pipe_); pipe_ = INVALID_HANDLE_VALUE; }
}

bool DiscordIPC::writeFrame(int opcode, const std::string& payload) {
    if (pipe_ == INVALID_HANDLE_VALUE) return false;
    std::vector<char> buf(8 + payload.size());
    int32_t op = (int32_t)opcode;
    int32_t len = (int32_t)payload.size();
    std::memcpy(buf.data(), &op, 4);
    std::memcpy(buf.data() + 4, &len, 4);
    std::memcpy(buf.data() + 8, payload.data(), payload.size());
    DWORD written = 0;
    if (!WriteFile(pipe_, buf.data(), (DWORD)buf.size(), &written, nullptr)
        || written != buf.size()) {
        logger::write("WARN", "IPC writeFrame failed");
        return false;
    }
    return true;
}

bool DiscordIPC::readFrame(std::string& outPayload) {
    if (pipe_ == INVALID_HANDLE_VALUE) return false;
    char header[8];
    DWORD got = 0;
    if (!ReadFile(pipe_, header, 8, &got, nullptr) || got != 8) return false;
    int32_t len = 0;
    std::memcpy(&len, header + 4, 4);
    if (len < 0 || len > 1 << 20) return false;
    outPayload.resize((size_t)len);
    if (len > 0) {
        if (!ReadFile(pipe_, &outPayload[0], (DWORD)len, &got, nullptr) || got != (DWORD)len)
            return false;
    }
    return true;
}

bool DiscordIPC::setActivity(const std::string& state, long long startUnix) {
    if (!isConnected()) return false;
    std::string payload =
        "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + std::to_string((int)GetCurrentProcessId()) +
        ",\"activity\":{\"state\":\"" + jsonEscape(state) +
        "\",\"timestamps\":{\"start\":" + std::to_string(startUnix) + "}}},"
        "\"nonce\":\"" + std::to_string(++nonce_) + "\"}";
    if (!writeFrame(1 /*FRAME*/, payload)) return false;
    std::string resp;
    readFrame(resp); // best-effort; ignore content
    return true;
}
```

- [ ] **Step 6: Run unit test to verify jsonEscape passes**

```powershell
cmake --build build --config Debug --target unit_tests
.\build\Debug\unit_tests.exe
```
Expected: `ALL TESTS PASSED`.

- [ ] **Step 7: Write the IPC host test (manual integration with real Discord)**

Create `test/ipc_host_test.cpp`:
```cpp
// Standalone host: verifies the IPC path end-to-end WITHOUT the game.
// Run with Discord open and logged in; check your profile shows the activity.
#include "../src/DiscordIPC.h"
#include "../src/Logger.h"
#include <cstdio>
#include <ctime>
#include <thread>
#include <chrono>

int main() {
    logger::init(L".");
    DiscordIPC ipc("1520195420398289017");
    if (!ipc.connect()) {
        std::printf("Could not connect to Discord IPC. Is Discord running?\n");
        return 1;
    }
    long long start = (long long)std::time(nullptr);
    if (!ipc.setActivity("Probando IPC (host de test)", start)) {
        std::printf("setActivity failed\n");
        return 1;
    }
    std::printf("Activity sent. Check your Discord profile. Holding 20s...\n");
    std::this_thread::sleep_for(std::chrono::seconds(20));
    return 0;
}
```

- [ ] **Step 8: Build the host test and run it manually**

```powershell
cmake --build build --config Debug --target ipc_host_test
.\build\Debug\ipc_host_test.exe
```
Expected (Discord running): prints "Activity sent..." and your Discord profile shows **Playing <app name>** with "Probando IPC (host de test)" for 20 seconds. If Discord is closed, it prints the "Is Discord running?" message and exits 1 — that is also acceptable (proves graceful failure).

- [ ] **Step 9: Commit**

```powershell
git add src/DiscordIPC.h src/DiscordIPC.cpp test/ipc_host_test.cpp test/unit_tests.cpp CMakeLists.txt
git commit -m "feat: native Discord IPC (handshake + set_activity) with host test"
```

---

### Task 4: GameState (memory read + scene/district translation)

**Files:**
- Modify: `src/GameState.h` (replace stub)
- Modify: `src/GameState.cpp` (replace stub)
- Modify: `test/unit_tests.cpp` (add cases + calls)

**Interfaces:**
- Consumes: `Config`, `logger`.
- Produces:
  ```cpp
  enum class Scene { Unknown, Menu, Loading, InGame };

  struct GameState {
      Scene scene = Scene::Unknown;
      int districtId = -1;
  };

  // Reads scene/district from a memory base using config offsets.
  // memoryBase: result of GetModuleHandleA("godfather.exe") at runtime, or a
  // test buffer in unit tests. Reads are SEH-guarded; on any failure the field
  // is left at its degraded default (scene=InGame, districtId=-1).
  GameState readGameState(const unsigned char* memoryBase, const Config& cfg);

  // Builds the Spanish RPC string from a GameState + the config district table.
  std::string describeState(const GameState& s, const Config& cfg);

  // Raw scene-code -> Scene mapping (the value the game stores at scene_offset).
  // 0 = Menu, 1 = Loading, 2 = InGame; anything else = Unknown.
  Scene sceneFromCode(int code);
  ```
  Convention for the in-memory scene code is documented in `docs/OFFSETS.md` and adjustable there if reverse engineering reveals different values; `sceneFromCode` is the single place to change it.

- [ ] **Step 1: Write the failing tests**

Append to `test/unit_tests.cpp` (before `main`):
```cpp
#include "../src/GameState.h"

static void test_scene_from_code() {
    CHECK(sceneFromCode(0) == Scene::Menu);
    CHECK(sceneFromCode(1) == Scene::Loading);
    CHECK(sceneFromCode(2) == Scene::InGame);
    CHECK(sceneFromCode(99) == Scene::Unknown);
}

static void test_read_degraded_when_offsets_unset() {
    Config cfg; cfg.sceneOffset = -1; cfg.districtOffset = -1;
    unsigned char buf[16] = {0};
    GameState s = readGameState(buf, cfg);
    CHECK(s.scene == Scene::InGame); // degraded default
    CHECK(s.districtId == -1);
}

static void test_read_from_simulated_memory() {
    Config cfg;
    cfg.sceneOffset = 0;     // scene code at base+0
    cfg.districtOffset = 4;  // district id at base+4
    unsigned char buf[16] = {0};
    *reinterpret_cast<int*>(buf + 0) = 2; // InGame
    *reinterpret_cast<int*>(buf + 4) = 3; // Brooklyn
    GameState s = readGameState(buf, cfg);
    CHECK(s.scene == Scene::InGame);
    CHECK(s.districtId == 3);
}

static void test_describe_in_game_with_district() {
    Config cfg; // default 9 districts
    GameState s; s.scene = Scene::InGame; s.districtId = 0;
    CHECK(describeState(s, cfg) == "En Hell's Kitchen");
}

static void test_describe_menu_and_loading() {
    Config cfg;
    GameState m; m.scene = Scene::Menu;
    CHECK(describeState(m, cfg) == "En el menú principal");
    GameState l; l.scene = Scene::Loading;
    CHECK(describeState(l, cfg) == "Cargando...");
}

static void test_describe_in_game_unknown_district() {
    Config cfg;
    GameState s; s.scene = Scene::InGame; s.districtId = -1;
    CHECK(describeState(s, cfg) == "Jugando a El Padrino");
}
```
Add to `main()`:
```cpp
    test_scene_from_code();
    test_read_degraded_when_offsets_unset();
    test_read_from_simulated_memory();
    test_describe_in_game_with_district();
    test_describe_menu_and_loading();
    test_describe_in_game_unknown_district();
```

- [ ] **Step 2: Run test to verify it fails**

```powershell
cmake --build build --config Debug --target unit_tests
.\build\Debug\unit_tests.exe
```
Expected: compile/link errors for the new GameState symbols. Confirms tests are real.

- [ ] **Step 3: Write the GameState header**

Replace `src/GameState.h`:
```cpp
#pragma once
#include <string>
#include "Config.h"

enum class Scene { Unknown, Menu, Loading, InGame };

struct GameState {
    Scene scene = Scene::Unknown;
    int districtId = -1;
};

Scene sceneFromCode(int code);
GameState readGameState(const unsigned char* memoryBase, const Config& cfg);
std::string describeState(const GameState& s, const Config& cfg);
```

- [ ] **Step 4: Implement GameState**

Replace `src/GameState.cpp`:
```cpp
#include "GameState.h"
#include "Logger.h"
#include <windows.h>

Scene sceneFromCode(int code) {
    switch (code) {
        case 0: return Scene::Menu;
        case 1: return Scene::Loading;
        case 2: return Scene::InGame;
        default: return Scene::Unknown;
    }
}

namespace {
    // SEH-guarded 4-byte read. Returns false on access violation.
    bool safeReadInt(const unsigned char* addr, int& out) {
        __try {
            out = *reinterpret_cast<const int*>(addr);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }
}

GameState readGameState(const unsigned char* memoryBase, const Config& cfg) {
    GameState s;
    // Degraded default: assume in-game without district when we can't read.
    s.scene = Scene::InGame;
    s.districtId = -1;

    if (memoryBase == nullptr) return s;

    if (cfg.sceneOffset >= 0) {
        int code = 0;
        if (safeReadInt(memoryBase + cfg.sceneOffset, code)) {
            Scene sc = sceneFromCode(code);
            if (sc != Scene::Unknown) s.scene = sc;
        }
    }
    if (cfg.districtOffset >= 0) {
        int id = -1;
        if (safeReadInt(memoryBase + cfg.districtOffset, id)) {
            s.districtId = id;
        }
    }
    return s;
}

std::string describeState(const GameState& s, const Config& cfg) {
    switch (s.scene) {
        case Scene::Menu:    return "En el menú principal";
        case Scene::Loading: return "Cargando...";
        case Scene::InGame:
        case Scene::Unknown:
        default: {
            auto it = cfg.districts.find(s.districtId);
            if (s.districtId >= 0 && it != cfg.districts.end())
                return "En " + it->second;
            return "Jugando a El Padrino";
        }
    }
}
```

- [ ] **Step 5: Run test to verify it passes**

```powershell
cmake --build build --config Debug --target unit_tests
.\build\Debug\unit_tests.exe
```
Expected: `ALL TESTS PASSED`.

> Note on encoding: `"En el menú principal"` contains a non-ASCII `ú`. Save `GameState.cpp` and `unit_tests.cpp` as **UTF-8**. If the comparison fails only on that test, the source encoding is the cause — re-save as UTF-8 (no BOM) and rebuild.

- [ ] **Step 6: Commit**

```powershell
git add src/GameState.h src/GameState.cpp test/unit_tests.cpp
git commit -m "feat: game-state memory read (SEH-guarded) + Spanish state strings"
```

---

### Task 5: dllmain (entry point + background poll loop)

**Files:**
- Create: `src/dllmain.cpp`

**Interfaces:**
- Consumes: `Config`, `GameState`, `DiscordIPC`, `logger`.
- Produces: the loadable `.asi`. No symbols consumed by other tasks.

- [ ] **Step 1: Implement dllmain**

Create `src/dllmain.cpp`:
```cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <ctime>
#include <atomic>
#include <thread>
#include "Config.h"
#include "GameState.h"
#include "DiscordIPC.h"
#include "Logger.h"

namespace {
    std::atomic<bool> g_running{false};
    std::thread g_thread;
    HMODULE g_self = nullptr;

    std::wstring moduleDir(HMODULE mod) {
        wchar_t path[MAX_PATH] = {0};
        GetModuleFileNameW(mod, path, MAX_PATH);
        std::wstring p(path);
        auto slash = p.find_last_of(L"\\/");
        return slash == std::wstring::npos ? L"." : p.substr(0, slash);
    }

    void pollLoop() {
        std::wstring dir = moduleDir(g_self);
        logger::init(dir);
        logger::write("INFO", "GodfatherDiscordRPC started");

        Config cfg = loadConfig(dir + L"\\GodfatherDiscordRPC.ini");
        DiscordIPC ipc(cfg.clientId);
        const long long sessionStart = (long long)std::time(nullptr);

        const auto* base = reinterpret_cast<const unsigned char*>(GetModuleHandleA("godfather.exe"));
        if (!base) logger::write("WARN", "godfather.exe module not found; degraded mode");

        std::string lastSent;
        long long lastSentAt = 0;

        while (g_running.load()) {
            if (!ipc.isConnected()) ipc.connect(); // no-op/sleep handled by interval

            GameState st = readGameState(base, cfg);
            std::string desc = describeState(st, cfg);
            long long now = (long long)std::time(nullptr);

            // Send only on change AND respecting Discord's ~15s rate limit.
            if (ipc.isConnected() && desc != lastSent && (now - lastSentAt) >= 15) {
                if (ipc.setActivity(desc, sessionStart)) {
                    lastSent = desc;
                    lastSentAt = now;
                    logger::write("INFO", "activity: " + desc);
                } else {
                    ipc.disconnect(); // force reconnect next loop
                }
            }

            // Sleep in small slices so shutdown is responsive.
            int slept = 0;
            while (g_running.load() && slept < cfg.pollIntervalMs) {
                Sleep(100);
                slept += 100;
            }
        }
        ipc.disconnect();
        logger::write("INFO", "GodfatherDiscordRPC stopped");
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            g_self = hModule;
            DisableThreadLibraryCalls(hModule);
            g_running.store(true);
            // Never block the loader lock: spawn a detached thread.
            g_thread = std::thread(pollLoop);
            g_thread.detach();
            break;
        case DLL_PROCESS_DETACH:
            g_running.store(false);
            // Thread is detached; it observes g_running and exits on its own.
            break;
    }
    return TRUE;
}
```

- [ ] **Step 2: Build the .asi**

```powershell
cmake --build build --config Release --target GodfatherDiscordRPC
```
Expected: produces `build\Release\GodfatherDiscordRPC.asi`. No build errors.

- [ ] **Step 3: Verify the binary is 32-bit**

```powershell
# dumpbin ships with MSVC. /headers shows "machine (x86)".
dumpbin /headers .\build\Release\GodfatherDiscordRPC.asi | Select-String "machine"
```
Expected: a line containing `machine (x86)`. If it says `x64`, the CMake generator was not configured with `-A Win32` — delete `build/`, re-run `cmake -S . -B build -A Win32`, rebuild.

- [ ] **Step 4: Commit**

```powershell
git add src/dllmain.cpp
git commit -m "feat: .asi entry point with background poll loop and reconnect"
```

---

### Task 6: Shipped `.ini`, OFFSETS guide, README, and deploy

**Files:**
- Create: `dist/GodfatherDiscordRPC.ini`
- Create: `docs/OFFSETS.md`
- Create: `README.md`

**Interfaces:**
- Consumes: the built `.asi` from Task 5.
- Produces: a deployable package (`.asi` + `.ini`) and documentation.

- [ ] **Step 1: Create the shipped ini**

Create `dist/GodfatherDiscordRPC.ini`:
```ini
[Discord]
client_id = 1520195420398289017

[Settings]
poll_interval_ms = 2000

[Memory]
; Offsets relativos a la base del módulo godfather.exe (decimal o 0x hex).
; Vacíos = modo degradado (muestra "Jugando a El Padrino" sin distrito ni menú).
; Rellenar tras encontrarlos con Cheat Engine. Ver docs/OFFSETS.md.
scene_offset    =
district_offset =

[Districts]
; districtId -> nombre mostrado. IDs provisionales hasta confirmar el orden real.
0 = Hell's Kitchen
1 = Midtown
2 = SoHo
3 = Brooklyn
4 = New Jersey
5 = Holland Tunnel
6 = Lincoln Tunnel
7 = Westside Parkway
8 = Woltz Compound
```

- [ ] **Step 2: Create the OFFSETS guide**

Create `docs/OFFSETS.md`:
```markdown
# Encontrar los offsets con Cheat Engine

Los offsets son **relativos a la base del módulo `godfather.exe`** (el plugin hace
`base = GetModuleHandle("godfather.exe")` y lee `base + offset`).

## 1. Base del módulo
- Abre Cheat Engine, adjúntalo a `godfather.exe`.
- En la lista de módulos (View → Enumerate modules), anota la dirección base de
  `godfather.exe` (p. ej. `0x00400000`). Llámala `BASE`.

## 2. district_offset
- Entra a un distrito conocido. Haz "Unknown initial value" → cambia de distrito →
  "Changed value", y repite hasta acotar a una dirección estable que cambie 1:1 con el
  distrito (un int pequeño).
- Cuando tengas la dirección `ADDR`, el offset es `ADDR - BASE`. Ponlo en el `.ini` como
  hex: `district_offset = 0x....`.
- Anota qué número corresponde a cada distrito y ajusta `[Districts]` si el orden no
  coincide con el provisional.

## 3. scene_offset
- Igual, pero alternando entre menú principal, pantalla de carga y partida. Busca un int
  que valga, por convención, 0=menú, 1=carga, 2=partida.
- Si los valores reales son distintos, NO cambies el `.ini`: ajusta la única función
  `sceneFromCode()` en `src/GameState.cpp` (es el punto único de traducción) y recompila.
- Offset: `ADDR - BASE`, ponlo en `scene_offset`.

## 4. Validar
- Rellena ambos offsets en el `.ini` junto al `.asi`, arranca el juego y recorre distritos.
- Revisa `GodfatherDiscordRPC.log` (junto al `.asi`): cada cambio se registra como
  `[INFO] activity: En <distrito>`.

> Si el juego se actualiza/parchea y el exe cambia, los offsets pueden moverse: vuelve a
> repetir este proceso. El plugin sigue funcionando en modo degradado mientras tanto.
```

- [ ] **Step 3: Create the README**

Create `README.md`:
```markdown
# Godfather Discord Rich Presence

Discord Rich Presence para *The Godfather: The Game* (PC, 2006). Plugin `.asi` nativo que
muestra en Discord si estás en menú, cargando o en partida, y en qué distrito.

## Requisitos
- El juego con el Ultimate ASI Loader ya presente (`dinput8.dll`). Esta instalación ya lo tiene.
- Discord de escritorio abierto y con sesión iniciada.

## Compilar (MSVC + CMake, x86)
```powershell
cmake -S . -B build -A Win32
cmake --build build --config Release --target GodfatherDiscordRPC
```
El resultado es `build\Release\GodfatherDiscordRPC.asi`.

## Instalar
1. Copia `GodfatherDiscordRPC.asi` a la carpeta `scripts\` del juego (junto a
   `SilentPatchGF.asi`).
2. Copia `dist\GodfatherDiscordRPC.ini` a esa misma carpeta `scripts\`.
3. Arranca el juego con Discord abierto.

Sin offsets configurados, muestra "Jugando a El Padrino". Para mostrar el distrito y el
estado menú/partida, rellena los offsets en el `.ini` siguiendo `docs/OFFSETS.md`.

## Tests
```powershell
cmake --build build --config Debug --target unit_tests ; .\build\Debug\unit_tests.exe
cmake --build build --config Debug --target ipc_host_test ; .\build\Debug\ipc_host_test.exe  # con Discord abierto
```

## Logs
`GodfatherDiscordRPC.log` se crea junto al `.asi`.
```

- [ ] **Step 4: Manual end-to-end deploy test**

Replace `<GAME>` with the actual game folder (where `godfather.exe` lives).
```powershell
Copy-Item .\build\Release\GodfatherDiscordRPC.asi "<GAME>\scripts\"
Copy-Item .\dist\GodfatherDiscordRPC.ini "<GAME>\scripts\"
```
Then launch the game with Discord open. Expected: your Discord profile shows **Playing <app name>** with "Jugando a El Padrino" (degraded, offsets still blank). Confirm `<GAME>\scripts\GodfatherDiscordRPC.log` contains `GodfatherDiscordRPC started` and `Discord IPC connected`.

- [ ] **Step 5: Commit**

```powershell
git add dist/GodfatherDiscordRPC.ini docs/OFFSETS.md README.md
git commit -m "docs: shipped ini, Cheat Engine offsets guide, README + deploy steps"
```

---

## Self-Review Notes

- **Spec coverage:** Config (Task 2), GameState memory+degraded mode (Task 4), DiscordIPC native protocol (Task 3), dllmain loop+errors+reconnect (Task 5), shipped ini with 9 districts + OFFSETS guide (Task 6), logging (Task 1). All spec sections map to a task.
- **Type consistency:** `Config`, `Scene`, `GameState`, `readGameState`, `describeState`, `sceneFromCode`, `DiscordIPC::{connect,disconnect,setActivity,isConnected}`, `jsonEscape`, `logger::{init,write}` are used identically across tasks.
- **Degraded mode** is exercised by a unit test (Task 4, step 1) and by the deploy test (Task 6) which runs with blank offsets.
- **32-bit constraint** is verified explicitly in Task 5, step 3 (`dumpbin /headers`).
- **No placeholders:** every code step contains complete, compilable code.
