cmake_minimum_required(VERSION 3.20)
project(GodfatherDiscordRPC CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- The .asi plugin (a DLL renamed to .asi) ---
add_library(GodfatherDiscordRPC SHARED
    src/dllmain.cpp
    src/Config.cpp
    src/DiscordIPC.cpp
    src/Logger.cpp
)
set_target_properties(GodfatherDiscordRPC PROPERTIES SUFFIX ".asi" PREFIX "")

# --- Unit tests (console exe, no game needed) ---
add_executable(unit_tests
    test/unit_tests.cpp
    src/Config.cpp
    src/DiscordIPC.cpp
    src/Logger.cpp
)

# --- IPC host test (console exe, talks to Discord without the game) ---
add_executable(ipc_host_test
    test/ipc_host_test.cpp
    src/DiscordIPC.cpp
    src/Logger.cpp
)
