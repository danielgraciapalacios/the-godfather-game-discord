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
