#pragma once
#include <string>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Escapes a string for embedding inside a JSON string literal.
std::string jsonEscape(const std::string& s);

// Minimal native Discord IPC client over the local named pipe.
// No external dependencies; speaks the handshake + SET_ACTIVITY protocol.
class DiscordIPC {
public:
    explicit DiscordIPC(std::string clientId);
    ~DiscordIPC();

    bool isConnected() const { return pipe_ != INVALID_HANDLE_VALUE; }
    bool connect();                 // opens pipe (-0..-9) + handshake; idempotent
    void disconnect();
    bool setActivity(const std::string& state, long long startUnix);

private:
    bool writeFrame(int opcode, const std::string& payload);
    bool readFrame(std::string& outPayload);

    std::string clientId_;
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
    long long nonce_ = 0;
};
