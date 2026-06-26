#pragma once
#include <string>

// Minimal append-only logger to a file next to the .asi.
namespace logger {
    void init(const std::wstring& dllDir);
    void write(const char* level, const std::string& msg);
}
