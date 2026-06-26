#include "Config.h"
#include "Logger.h"
#include <fstream>
#include <algorithm>
#include <cctype>

namespace {
    std::string trim(std::string s) {
        auto notSpace = [](int ch){ return !std::isspace(ch); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
        s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
        return s;
    }
}

Config loadConfig(const std::wstring& iniPath) {
    Config c;
    std::ifstream f(iniPath);
    if (!f) {
        logger::write("WARN", "ini not found, using defaults");
        return c;
    }

    std::string section, line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (section == "Discord" && key == "client_id" && !val.empty()) {
            c.clientId = val;
        } else if (section == "Settings" && key == "poll_interval_ms" && !val.empty()) {
            try { int v = std::stoi(val); if (v > 0) c.pollIntervalMs = v; } catch (...) {}
        } else if (section == "Settings" && key == "state" && !val.empty()) {
            c.fixedState = val;
        }
    }
    return c;
}
