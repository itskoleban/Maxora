#include "pawn_bridge.hpp"
#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

#ifndef MAXORA_FIXTURE_DIR
#define MAXORA_FIXTURE_DIR "lib/libmaxminddb/t/maxmind-db/test-data"
#endif
namespace {
using namespace maxora;
std::size_t checks = 0;
void check(bool value, int line) {
    ++checks;
    if (!value) throw std::runtime_error("native test failure at line " + std::to_string(line));
}
#define CHECK(value) check((value), __LINE__)
struct Vm {
    std::array<cell, 8192> data{};
    AMX amx{};
    Vm() {
        amx.data = reinterpret_cast<unsigned char*>(data.data());
        amx.hea = 16384; amx.stk = 24576; amx.stp = 32768;
    }
    void string(cell at, std::string_view text) {
        for (std::size_t i = 0; i < text.size(); ++i) data[static_cast<std::size_t>(at) / 4 + i] = static_cast<unsigned char>(text[i]);
        data[static_cast<std::size_t>(at) / 4 + text.size()] = 0;
    }
    Status call(const char* name, std::initializer_list<cell> args) {
        std::vector<cell> params{static_cast<cell>(args.size() * 4)};
        params.insert(params.end(), args);
        for (int i = 0; i < native_count(); ++i)
            if (std::strcmp(native_table()[i].name, name) == 0)
                return static_cast<Status>(native_table()[i].func(&amx, params.data()));
        throw std::runtime_error("native missing");
    }
};
}
int main() {
    try {
        Service service;
        PawnBridge bridge(service);
        Vm a, b;
        CHECK(bridge.attach(&a.amx) == Status::Ok);
        CHECK(bridge.attach(&b.amx) == Status::Ok);
        std::set<std::string> names;
        for (int i = 0; i < native_count(); ++i) {
            CHECK(names.insert(native_table()[i].name).second);
            CHECK(a.call(native_table()[i].name, {}) == Status::InvalidArgument);
        }
        CHECK(native_count() == 19);
        const auto path = (std::filesystem::path(MAXORA_FIXTURE_DIR) / "MaxMind-DB-test-decoder.mmdb").string();
        a.string(0, path);
        CHECK(a.call("Maxora_Load", {0, 4096}) == Status::Ok);
        const auto db = a.data[1024];
        CHECK(db > 0);
        CHECK(a.call("Maxora_IsValid", {db, 4100}) == Status::Ok && a.data[1025] == 1);
        CHECK(b.call("Maxora_IsValid", {db, 4100}) == Status::WrongOwner);
        a.string(2048, "1.1.1.1");
        CHECK(a.call("Maxora_Lookup", {db, 2048, 4104}) == Status::Ok);
        const auto result = a.data[1026];
        a.string(3072, "/uint16");
        CHECK(a.call("Maxora_GetInt", {result, 3072, 4108}) == Status::Ok && a.data[1027] == 100);
        CHECK(a.call("Maxora_GetType", {result, 3072, 4108}) == Status::Ok && a.data[1027] == MMDB_DATA_TYPE_UINT16);
        CHECK(a.call("Maxora_GetText", {result, 3072, 5000, 8, 4108}) == Status::Ok && a.data[1250] == '1');
        a.string(3072, "/utf8_string");
        a.data[1250] = 777;
        CHECK(a.call("Maxora_GetString", {result, 3072, 5000, 1, 4108}) == Status::BufferTooSmall);
        CHECK(a.data[1250] == 777 && a.data[1027] > 1);
        CHECK(a.call("Maxora_GetString", {result, 3072, 5000, 100, 4108}) == Status::Ok);
        CHECK(a.call("Maxora_GetSize", {result, 3072, 4108}) == Status::Ok && a.data[1027] > 0);
        CHECK(a.call("Maxora_GetString", {result, 3072, 5000, 100, 5000}) == Status::InvalidArgument);
        CHECK(a.call("Maxora_GetString", {result, 3072, 16380, 2, 4108}) == Status::InvalidArgument);
        CHECK(a.call("Maxora_GetString", {result, 3072, 32764, 2, 4108}) == Status::InvalidArgument);
        CHECK(a.call("Maxora_GetString", {result, 3072, 5000, -1, 4108}) == Status::InvalidArgument);
        CHECK(a.call("Maxora_GetInt", {result, 3072, 16384}) == Status::InvalidArgument);
        CHECK(a.call("Maxora_GetInt", {result, 3072, 4109}) == Status::InvalidArgument);
        a.string(3072, "/boolean");
        CHECK(a.call("Maxora_GetBoolean", {result, 3072, 4108}) == Status::Ok);
        a.string(3072, "/double");
        CHECK(a.call("Maxora_GetFloat", {result, 3072, 4108}) == Status::Ok);
        a.string(3072, "/absent");
        CHECK(a.call("Maxora_HasField", {result, 3072, 4108}) == Status::Ok && a.data[1027] == 0);
        a.string(3072, "malformed");
        CHECK(a.call("Maxora_HasField", {result, 3072, 4108}) == Status::InvalidPath);
        CHECK(a.call("Maxora_GetPrefix", {result, 4108}) == Status::Ok);
        CHECK(a.call("Maxora_GetDatabaseType", {db, 5000, 100, 4108}) == Status::Ok);
        CHECK(a.call("Maxora_GetBuildEpoch", {db, 5000, 100, 4108}) == Status::Ok);
        CHECK(a.call("Maxora_GetIpVersion", {db, 4108}) == Status::Ok);
        CHECK(a.call("Maxora_GetStatusMessage", {0, 5000, 100, 4108}) == Status::Ok);
        CHECK(a.call("Maxora_GetStatusMessage", {999, 5000, 100, 4108}) == Status::InvalidArgument);
        a.string(2048, "localhost");
        CHECK(a.call("Maxora_Lookup", {db, 2048, 4104}) == Status::InvalidIp);
        // Invalid AMX ranges, unterminated and packed strings are never scanned.
        a.data[0] = 0x41424344;
        CHECK(a.call("Maxora_Reload", {db, 0}) == Status::InvalidArgument);
        a.data[4095] = 'a';
        CHECK(a.call("Maxora_Reload", {db, 16380}) == Status::InvalidArgument);
        a.string(0, "missing.mmdb");
        CHECK(a.call("Maxora_Reload", {db, 0}) == Status::OpenFailed);
        CHECK(a.call("Maxora_IsValid", {db, 4100}) == Status::Ok && a.data[1025] == 1);
        a.string(0, path);
        CHECK(a.call("Maxora_Reload", {db, 0}) == Status::Ok);
        CHECK(a.call("Maxora_Unload", {db}) == Status::Ok);
        CHECK(a.call("Maxora_GetPrefix", {result, 4108}) == Status::Ok);
        CHECK(a.call("Maxora_Release", {result}) == Status::Ok);
        CHECK(a.call("Maxora_Release", {result}) == Status::InvalidHandle);
        CHECK(a.call("Maxora_Load", {0, 4096}) == Status::Ok);
        const auto next = a.data[1024];
        CHECK(next != db);
        CHECK(a.call("Maxora_Unload", {db}) == Status::InvalidHandle);
        bridge.detach(&a.amx);
        CHECK(service.size() == 0);
        CHECK(a.call("Maxora_Unload", {next}) == Status::InvalidArgument);
        CHECK(bridge.attach(&a.amx) == Status::Ok);
        CHECK(a.call("Maxora_Unload", {next}) == Status::InvalidHandle);
        std::cout << checks << " native checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
