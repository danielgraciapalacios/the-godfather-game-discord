// Standalone host: verifies the Discord IPC path end-to-end WITHOUT the game.
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
        std::printf("No se pudo conectar al IPC de Discord. Esta Discord abierto?\n");
        return 1;
    }
    long long start = (long long)std::time(nullptr);
    if (!ipc.setActivity("Nueva York", start)) {
        std::printf("setActivity fallo\n");
        return 1;
    }
    std::printf("Actividad enviada. Mira tu perfil de Discord. Esperando 20s...\n");
    std::this_thread::sleep_for(std::chrono::seconds(20));
    return 0;
}
