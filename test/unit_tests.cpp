#include <cassert>
#include <cstdio>
#include <string>
#include <fstream>
#include "../src/Logger.h"
#include "../src/Config.h"
#include "../src/DiscordIPC.h"

static int g_failures = 0;
#define CHECK(cond) do { if(!(cond)) { std::printf("FAIL: %s (line %d)\n", #cond, __LINE__); ++g_failures; } } while(0)

static void writeFile(const char* path, const std::string& content) {
    std::ofstream f(path); f << content;
}

static void test_logger_writes_line() {
    logger::init(L".");
    logger::write("INFO", "hello world");
    std::ifstream f("GodfatherDiscordRPC.log");
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(content.find("[INFO] hello world") != std::string::npos);
}

static void test_config_defaults_when_missing() {
    Config c = loadConfig(L"does_not_exist.ini");
    CHECK(c.clientId == "1520195420398289017");
    CHECK(c.pollIntervalMs == 2000);
    CHECK(c.fixedState == "Nueva York");
}

static void test_config_parses_values() {
    writeFile("t2.ini",
        "[Discord]\nclient_id = 123\n"
        "[Settings]\npoll_interval_ms = 500\nstate = Brooklyn\n");
    Config c = loadConfig(L"t2.ini");
    CHECK(c.clientId == "123");
    CHECK(c.pollIntervalMs == 500);
    CHECK(c.fixedState == "Brooklyn");
}

static void test_json_escape() {
    CHECK(jsonEscape("Hell's Kitchen") == "Hell's Kitchen"); // apostrophe is legal raw in JSON
    CHECK(jsonEscape("a\"b") == "a\\\"b");
    CHECK(jsonEscape("a\\b") == "a\\\\b");
    CHECK(jsonEscape(std::string("a\nb")) == "a\\nb");
}

int main() {
    test_logger_writes_line();
    test_config_defaults_when_missing();
    test_config_parses_values();
    test_json_escape();
    if (g_failures == 0) std::printf("ALL TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
