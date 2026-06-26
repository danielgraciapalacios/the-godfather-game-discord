#include "DiscordIPC.h"
#include "Logger.h"
#include <cstdint>
#include <cstring>
#include <cstdio>
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
    if (len < 0 || len > (1 << 20)) return false;
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
