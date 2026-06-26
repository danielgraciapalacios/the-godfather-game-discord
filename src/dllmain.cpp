#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <ctime>
#include <atomic>
#include <thread>
#include "Config.h"
#include "DiscordIPC.h"
#include "Logger.h"

namespace {
    std::atomic<bool> g_running{false};
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
        logger::write("INFO", "GodfatherDiscordRPC started (basic version)");

        Config cfg = loadConfig(dir + L"\\GodfatherDiscordRPC.ini");
        DiscordIPC ipc(cfg.clientId);
        const long long sessionStart = (long long)std::time(nullptr);

        bool sent = false; // the fixed state only needs sending once per connection

        while (g_running.load()) {
            if (!ipc.isConnected()) {
                if (ipc.connect()) {
                    sent = false; // (re)send after a fresh connection
                }
            }

            if (ipc.isConnected() && !sent) {
                if (ipc.setActivity(cfg.fixedState, sessionStart)) {
                    sent = true;
                    logger::write("INFO", "activity: " + cfg.fixedState);
                } else {
                    ipc.disconnect(); // force reconnect next loop
                }
            }

            // Sleep in small slices so shutdown stays responsive.
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
            std::thread(pollLoop).detach();
            break;
        case DLL_PROCESS_DETACH:
            g_running.store(false);
            break;
    }
    return TRUE;
}
