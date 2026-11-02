#include "pawn_bridge.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string_view>

#ifndef MAXORA_FIXTURE_DIR
#define MAXORA_FIXTURE_DIR "lib/libmaxminddb/t/maxmind-db/test-data"
#endif

namespace {
using namespace maxora;

// The runtime owns these backing pointers and the parameters pointer in real
// native dispatch. Fuzz only Pawn-controlled cell contents and bounded VM
// region descriptors; never invent an invalid host allocation/pointer.
struct Vm {
    std::array<cell, 4096> data{};
    AMX amx{};
    Vm() {
        amx.data = reinterpret_cast<unsigned char*>(data.data());
        reset();
    }
    void reset() noexcept {
        data.fill(0);
        amx.hea = 8192;
        amx.stk = 12288;
        amx.stp = 16384;
    }
    void text(std::string_view value) noexcept {
        const auto count = std::min(value.size(), std::size_t{1024});
        for (std::size_t i = 0; i < count; ++i) data[i] = static_cast<unsigned char>(value[i]);
        data[count] = 0;
    }
};

class Input {
public:
    Input(const std::uint8_t* bytes, std::size_t size) : bytes_(bytes), size_(size) {}
    std::uint32_t word() noexcept {
        std::uint32_t value = 0;
        for (unsigned shift = 0; shift < 32 && position_ < size_; shift += 8)
            value |= static_cast<std::uint32_t>(bytes_[position_++]) << shift;
        return value;
    }
    cell number() noexcept {
        const auto bits = word();
        cell value;
        static_assert(sizeof(value) == sizeof(bits), "32-bit Pawn cell required");
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
private:
    const std::uint8_t* bytes_;
    std::size_t size_;
    std::size_t position_ = 0;
};

struct Harness {
    Service service;
    PawnBridge bridge{service};
    Vm vm, other;
    Handle database = 0, result = 0;
    Harness() {
        if (bridge.attach(&vm.amx) != Status::Ok || bridge.attach(&other.amx) != Status::Ok) std::abort();
        const auto owner = bridge.owner(&vm.amx);
        const auto path = (std::filesystem::path(MAXORA_FIXTURE_DIR) / "MaxMind-DB-test-decoder.mmdb").string();
        if (service.load(owner, path, database) != Status::Ok ||
            service.lookup(owner, database, "1.1.1.1", result) != Status::Ok) std::abort();
    }
};

struct Native {
    const char* name;
    std::size_t arguments;
    bool database;
};
constexpr std::array<Native, 14> cases{{
    {"Maxora_IsValid", 2, true},
    {"Maxora_GetString", 5, false}, {"Maxora_GetText", 5, false},
    {"Maxora_GetInt", 3, false}, {"Maxora_GetFloat", 3, false},
    {"Maxora_GetBoolean", 3, false}, {"Maxora_GetType", 3, false},
    {"Maxora_GetSize", 3, false}, {"Maxora_HasField", 3, false},
    {"Maxora_GetPrefix", 2, false}, {"Maxora_GetDatabaseType", 4, true},
    {"Maxora_GetBuildEpoch", 4, true}, {"Maxora_GetIpVersion", 2, true},
    {"Maxora_GetStatusMessage", 4, false}
}};
constexpr std::array<const char*, 14> paths{{
    "", "/utf8_string", "/uint16", "/uint32", "/uint64", "/uint128",
    "/int32", "/double", "/boolean", "/bytes", "/array/0", "/map/mapX/utf8_string",
    "/array/-1", "/absent"
}};
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* bytes, std::size_t size) {
    static Harness harness;
    auto& vm = harness.vm;
    vm.reset();
    harness.other.reset();
    Input input(bytes, size);
    const auto mode = input.word();
    const auto& selected = cases[input.word() % cases.size()];
    vm.text(paths[input.word() % paths.size()]);
    if (mode & 1U) {
        // Arbitrary packed/unpacked, terminated/unterminated and UTF-8 data.
        const auto count = std::min(size, vm.data.size() * sizeof(cell));
        if (count) std::memcpy(vm.data.data(), bytes, count);
    }
    if (mode & 2U) {
        // All descriptors remain within the real allocation. Ordering may be
        // deliberately invalid; the bridge must reject those spans.
        vm.amx.hea = static_cast<cell>(input.word() % 4097U) * 4;
        vm.amx.stk = static_cast<cell>(input.word() % 4097U) * 4;
        vm.amx.stp = 16384;
    }
    std::array<cell, 6> params{};
    params[0] = static_cast<cell>(selected.arguments * sizeof(cell));
    params[1] = selected.database ? harness.database : harness.result;
    params[2] = 0;
    params[3] = 4096;
    params[4] = 256;
    params[5] = 6144;
    if (selected.arguments == 2) params[2] = 6144;
    if (selected.arguments == 4) { params[2] = 4096; params[3] = 256; params[4] = 6144; }
    if (std::strcmp(selected.name, "Maxora_GetStatusMessage") == 0)
        params[1] = static_cast<cell>(input.word() % 20U);
    const auto mutations = input.word();
    for (std::size_t i = 0; i < params.size(); ++i)
        if (mutations & (1U << i)) params[i] = input.number();

    auto& active_vm = (mode & 4U) ? harness.other : vm;
    // The real AMX dispatch puts the count and argument cells in AMX memory.
    // A stack at STP deliberately leaves no readable parameter cell; a stack
    // near STP exposes only a truncated block. The host allocation stays valid.
    const auto offset = static_cast<std::size_t>(active_vm.amx.stk) / sizeof(cell);
    const auto available = active_vm.data.size() - offset;
    const auto copied = std::min(params.size(), available);
    std::copy_n(params.begin(), copied, active_vm.data.begin() + static_cast<std::ptrdiff_t>(offset));
    AMX* amx = &active_vm.amx;
    if (mode & 8U) amx = nullptr;
    const cell* arguments = (mode & 16U) ? nullptr : active_vm.data.data() + offset;
    for (int i = 0; i < native_count(); ++i) {
        if (std::strcmp(native_table()[i].name, selected.name) != 0) continue;
        const auto status = native_table()[i].func(amx, arguments);
        if (status < 0 || status > static_cast<cell>(Status::UnsupportedFamily)) std::abort();
        break;
    }
    // Resource mutation natives are excluded: fuzz bytes can never open an
    // arbitrary filesystem path or consume the process-lifetime handle budget.
    if (harness.service.size() != 2) std::abort();
    return 0;
}
