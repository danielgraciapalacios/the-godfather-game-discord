#pragma once
#include <string>

// Minimal config for the basic version. Memory offsets / districts come later.
struct Config {
    std::string clientId = "1520195420398289017";
    int pollIntervalMs = 2000;
    // Fixed state line shown in Discord for now. Will be replaced by the
    // per-district value once memory reading is implemented.
    std::string fixedState = "Nueva York";
};

Config loadConfig(const std::wstring& iniPath);
