#include "pawn_bridge.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace maxora {
namespace {
PawnBridge* route = nullptr;
static_assert(sizeof(cell) == 4, "Maxora's Pawn ABI requires 32-bit cells");

class Memory {
public:
    explicit Memory(AMX* amx) : amx_(amx) {}
    bool parameters(const cell* pointer, std::size_t count) const noexcept {
        if (!amx_ || !pointer) return false;
        unsigned char* data = amx_->data;
        if (!data && amx_->base) {
            const auto* header = reinterpret_cast<const AMX_HEADER*>(amx_->base);
            if (header->dat < 0) return false;
            data = amx_->base + header->dat;
        }
        if (!data) return false;
        const auto base = reinterpret_cast<std::uintptr_t>(data);
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        if (address < base || address - base > static_cast<std::uintptr_t>(std::numeric_limits<cell>::max()))
            return false;
        return cells(static_cast<cell>(address - base), count) == pointer;
    }
    cell* cells(cell address, std::size_t count = 1) const noexcept {
        if (!amx_ || address < 0 || address % 4 != 0 || count == 0 ||
            count > static_cast<std::size_t>(std::numeric_limits<cell>::max()) / 4)
            return nullptr;
        const auto start = static_cast<std::uint32_t>(address);
        const auto length = static_cast<std::uint32_t>(count * 4);
        const auto end = static_cast<std::uint64_t>(start) + length;
        if (amx_->hea < 0 || amx_->stk < amx_->hea || amx_->stp < amx_->stk ||
            !((end <= static_cast<std::uint32_t>(amx_->hea)) ||
              (start >= static_cast<std::uint32_t>(amx_->stk) &&
               end <= static_cast<std::uint32_t>(amx_->stp))))
            return nullptr;
        unsigned char* data = amx_->data;
        if (!data && amx_->base) {
            auto* header = reinterpret_cast<const AMX_HEADER*>(amx_->base);
            if (header->dat < 0) return nullptr;
            data = amx_->base + header->dat;
        }
        return data ? reinterpret_cast<cell*>(data + start) : nullptr;
    }
    Status string(cell address, std::size_t limit, std::string& out) const {
        out.clear();
        for (std::size_t i = 0; i <= limit; ++i) {
            const auto at = static_cast<std::int64_t>(address) + static_cast<std::int64_t>(i * 4);
            if (at > std::numeric_limits<cell>::max()) return Status::InvalidArgument;
            const auto* p = cells(static_cast<cell>(at));
            if (!p || *p < 0 || *p > 255) return Status::InvalidArgument;
            if (*p == 0) return valid_utf8(out) ? Status::Ok : Status::InvalidUtf8;
            if (i == limit) return Status::InvalidArgument;
            out.push_back(static_cast<char>(*p));
        }
        return Status::InvalidArgument;
    }
    Status output(cell address, cell capacity, cell required, std::string_view text) const noexcept {
        auto* needed = cells(required);
        if (!needed || capacity < 1 || capacity > 1048576 ||
            text.size() >= static_cast<std::size_t>(std::numeric_limits<cell>::max()))
            return Status::InvalidArgument;
        auto* destination = cells(address, static_cast<std::size_t>(capacity));
        if (!destination) return Status::InvalidArgument;
        const auto begin = static_cast<std::int64_t>(address);
        const auto end = begin + static_cast<std::int64_t>(capacity) * 4;
        if (required >= begin && required < end) return Status::InvalidArgument;
        *needed = static_cast<cell>(text.size() + 1);
        if (text.size() >= static_cast<std::size_t>(capacity)) return Status::BufferTooSmall;
        for (std::size_t i = 0; i < text.size(); ++i)
            destination[i] = static_cast<unsigned char>(text[i]);
        destination[text.size()] = 0;
        return Status::Ok;
    }
private:
    AMX* amx_;
};
struct Context { Service& service; Owner owner; Memory memory; const cell* p; };
template <std::size_t Count, typename Function>
cell invoke(AMX* amx, const cell* params, Function function) noexcept {
    if (!route || !amx || !params)
        return static_cast<cell>(Status::InvalidArgument);
    const Memory memory(amx);
    // The AMX interpreter passes its stack pointer, but the count cell is script
    // controlled. Validate the entire expected argument block before reading it.
    if (!memory.parameters(params, Count + 1) || params[0] != static_cast<cell>(Count * sizeof(cell)))
        return static_cast<cell>(Status::InvalidArgument);
    const auto owner = route->owner(amx);
    if (!owner) return static_cast<cell>(Status::InvalidArgument);
    try { return static_cast<cell>(function(Context{route->service(), owner, memory, params})); }
    catch (...) { return static_cast<cell>(Status::InternalError); }
}
Status get_value(Context& c, Value& value) {
    const Lookup* lookup = nullptr;
    auto status = c.service.result(c.owner, c.p[1], lookup);
    if (status != Status::Ok) return status;
    std::string path;
    status = c.memory.string(c.p[2], 1024, path);
    return status == Status::Ok ? lookup->get(path, value) : status;
}

cell AMX_NATIVE_CALL load(AMX* amx, const cell* p) {
    return invoke<2>(amx, p, [](Context c) {
        auto* out = c.memory.cells(c.p[2]);
        if (!out) return Status::InvalidArgument;
        std::string path;
        auto s = c.memory.string(c.p[1], 4095, path);
        if (s != Status::Ok) return s;
        Handle value = 0;
        s = c.service.load(c.owner, path, value);
        if (s == Status::Ok) *out = value;
        return s;
    });
}
cell AMX_NATIVE_CALL reload(AMX* amx, const cell* p) {
    return invoke<2>(amx, p, [](Context c) {
        std::string path;
        const auto s = c.memory.string(c.p[2], 4095, path);
        return s == Status::Ok ? c.service.reload(c.owner, c.p[1], path) : s;
    });
}
cell AMX_NATIVE_CALL unload(AMX* amx, const cell* p) {
    return invoke<1>(amx, p, [](Context c) { return c.service.unload(c.owner, c.p[1]); });
}
cell AMX_NATIVE_CALL valid(AMX* amx, const cell* p) {
    return invoke<2>(amx, p, [](Context c) {
        auto* out = c.memory.cells(c.p[2]);
        if (!out) return Status::InvalidArgument;
        bool value = false;
        const auto s = c.service.valid(c.owner, c.p[1], value);
        if (s == Status::Ok) *out = value ? 1 : 0;
        return s;
    });
}
cell AMX_NATIVE_CALL lookup(AMX* amx, const cell* p) {
    return invoke<3>(amx, p, [](Context c) {
        auto* out = c.memory.cells(c.p[3]);
        if (!out) return Status::InvalidArgument;
        std::string ip;
        auto s = c.memory.string(c.p[2], 45, ip);
        if (s != Status::Ok) return s;
        Handle value = 0;
        s = c.service.lookup(c.owner, c.p[1], ip, value);
        if (s == Status::Ok) *out = value;
        return s;
    });
}
cell AMX_NATIVE_CALL release(AMX* amx, const cell* p) {
    return invoke<1>(amx, p, [](Context c) { return c.service.release(c.owner, c.p[1]); });
}
template <bool Text>
cell AMX_NATIVE_CALL get_string(AMX* amx, const cell* p) {
    return invoke<5>(amx, p, [](Context c) {
        Value value;
        auto s = get_value(c, value);
        if (s != Status::Ok) return s;
        if constexpr (Text) {
            std::string text;
            s = value.text(text);
            return s == Status::Ok ? c.memory.output(c.p[3], c.p[4], c.p[5], text) : s;
        } else {
            std::string_view text;
            s = value.string(text);
            return s == Status::Ok ? c.memory.output(c.p[3], c.p[4], c.p[5], text) : s;
        }
    });
}
enum class Scalar { Integer, Floating, Boolean, Type, Size };
template <Scalar Kind>
cell AMX_NATIVE_CALL get_scalar(AMX* amx, const cell* p) {
    return invoke<3>(amx, p, [](Context c) {
        auto* out = c.memory.cells(c.p[3]);
        if (!out) return Status::InvalidArgument;
        Value value;
        auto s = get_value(c, value);
        if (s != Status::Ok) return s;
        std::int32_t number = 0;
        if constexpr (Kind == Scalar::Integer) s = value.integer(number);
        if constexpr (Kind == Scalar::Floating) {
            float f = 0;
            s = value.floating(f);
            std::memcpy(&number, &f, sizeof(number));
        }
        if constexpr (Kind == Scalar::Boolean) { bool b = false; s = value.boolean(b); number = b ? 1 : 0; }
        if constexpr (Kind == Scalar::Type) number = static_cast<std::int32_t>(value.data.type);
        if constexpr (Kind == Scalar::Size) {
            const auto t = value.data.type;
            if (t != MMDB_DATA_TYPE_MAP && t != MMDB_DATA_TYPE_ARRAY &&
                t != MMDB_DATA_TYPE_UTF8_STRING && t != MMDB_DATA_TYPE_BYTES)
                return Status::TypeMismatch;
            if (value.data.data_size > static_cast<std::uint32_t>(std::numeric_limits<cell>::max()))
                return Status::OutOfRange;
            number = static_cast<cell>(value.data.data_size);
        }
        if (s == Status::Ok) *out = number;
        return s;
    });
}
cell AMX_NATIVE_CALL has_field(AMX* amx, const cell* p) {
    return invoke<3>(amx, p, [](Context c) {
        auto* out = c.memory.cells(c.p[3]);
        if (!out) return Status::InvalidArgument;
        Value value;
        auto s = get_value(c, value);
        if (s == Status::NoField) { *out = 0; return Status::Ok; }
        if (s == Status::Ok) *out = 1;
        return s;
    });
}
cell AMX_NATIVE_CALL prefix(AMX* amx, const cell* p) {
    return invoke<2>(amx, p, [](Context c) {
        auto* out = c.memory.cells(c.p[2]);
        if (!out) return Status::InvalidArgument;
        const Lookup* value = nullptr;
        const auto s = c.service.result(c.owner, c.p[1], value);
        if (s == Status::Ok) *out = value->prefix;
        return s;
    });
}
template <bool Epoch>
cell AMX_NATIVE_CALL metadata_string(AMX* amx, const cell* p) {
    return invoke<4>(amx, p, [](Context c) {
        std::string type, epoch;
        std::int32_t version = 0;
        const auto s = c.service.metadata(c.owner, c.p[1], type, epoch, version);
        return s == Status::Ok ? c.memory.output(c.p[2], c.p[3], c.p[4], Epoch ? epoch : type) : s;
    });
}
cell AMX_NATIVE_CALL ip_version(AMX* amx, const cell* p) {
    return invoke<2>(amx, p, [](Context c) {
        auto* out = c.memory.cells(c.p[2]);
        if (!out) return Status::InvalidArgument;
        std::string type, epoch;
        std::int32_t version = 0;
        const auto s = c.service.metadata(c.owner, c.p[1], type, epoch, version);
        if (s == Status::Ok) *out = version;
        return s;
    });
}
cell AMX_NATIVE_CALL message(AMX* amx, const cell* p) {
    return invoke<4>(amx, p, [](Context c) {
        if (c.p[1] < 0 || c.p[1] > static_cast<cell>(Status::UnsupportedFamily)) return Status::InvalidArgument;
        return c.memory.output(c.p[2], c.p[3], c.p[4], status_message(static_cast<Status>(c.p[1])));
    });
}
const AMX_NATIVE_INFO natives[] = {
    {"Maxora_Load", load}, {"Maxora_Reload", reload}, {"Maxora_Unload", unload},
    {"Maxora_IsValid", valid}, {"Maxora_Lookup", lookup}, {"Maxora_Release", release},
    {"Maxora_GetString", get_string<false>}, {"Maxora_GetText", get_string<true>},
    {"Maxora_GetInt", get_scalar<Scalar::Integer>}, {"Maxora_GetFloat", get_scalar<Scalar::Floating>},
    {"Maxora_GetBoolean", get_scalar<Scalar::Boolean>}, {"Maxora_GetType", get_scalar<Scalar::Type>},
    {"Maxora_GetSize", get_scalar<Scalar::Size>}, {"Maxora_HasField", has_field},
    {"Maxora_GetPrefix", prefix}, {"Maxora_GetDatabaseType", metadata_string<false>},
    {"Maxora_GetBuildEpoch", metadata_string<true>}, {"Maxora_GetIpVersion", ip_version},
    {"Maxora_GetStatusMessage", message}, {nullptr, nullptr}
};
} // namespace
PawnBridge::PawnBridge(Service& service) : service_(service) {
    if (route) throw std::logic_error("Only one Maxora Pawn bridge may be active");
    route = this;
}
PawnBridge::~PawnBridge() { clear(); route = nullptr; }
Status PawnBridge::attach(AMX* amx) {
    if (!amx) return Status::InvalidArgument;
    if (scripts_.find(amx) != scripts_.end()) return Status::Ok;
    if (next_owner_ == 0 || next_owner_ == std::numeric_limits<Owner>::max()) return Status::ResourceLimit;
    scripts_.emplace(amx, next_owner_++);
    return Status::Ok;
}
void PawnBridge::detach(AMX* amx) noexcept {
    const auto it = scripts_.find(amx);
    if (it != scripts_.end()) { service_.clear_owner(it->second); scripts_.erase(it); }
}
void PawnBridge::clear() noexcept {
    for (const auto& pair : scripts_) service_.clear_owner(pair.second);
    scripts_.clear();
}
Owner PawnBridge::owner(AMX* amx) const noexcept {
    const auto it = scripts_.find(amx);
    return it == scripts_.end() ? 0 : it->second;
}
const AMX_NATIVE_INFO* native_table() noexcept { return natives; }
int native_count() noexcept { return static_cast<int>(std::size(natives) - 1); }
} // namespace maxora
