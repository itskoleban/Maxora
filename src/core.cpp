#include "core.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace maxora {
const char* status_message(Status status) noexcept {
    static constexpr const char* messages[] = {
        "Success", "Invalid argument or AMX memory", "Invalid or expired handle",
        "Resource belongs to another script", "Invalid numeric IP address", "Invalid field path",
        "Cannot open database file", "Invalid database", "Unsupported database format",
        "No record for address", "Field does not exist", "Value has a different type",
        "Value is outside the destination range", "Destination buffer is too small",
        "Invalid UTF-8 or embedded NUL", "Resource limit reached", "Database decoder error",
        "Internal failure", "IPv6 lookup in IPv4 database"
    };
    const auto n = static_cast<std::uint32_t>(status);
    return n < sizeof(messages) / sizeof(messages[0]) ? messages[n] : "Unknown status";
}

bool valid_utf8(std::string_view text) noexcept {
    std::size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<unsigned char>(text[i++]);
        if (lead == 0) return false;
        if (lead < 0x80) continue;
        unsigned count = 0;
        std::uint32_t value = 0, minimum = 0;
        if (lead >= 0xC2 && lead <= 0xDF) { count = 1; value = lead & 0x1FU; minimum = 0x80; }
        else if (lead >= 0xE0 && lead <= 0xEF) { count = 2; value = lead & 0x0FU; minimum = 0x800; }
        else if (lead >= 0xF0 && lead <= 0xF4) { count = 3; value = lead & 0x07U; minimum = 0x10000; }
        else return false;
        if (text.size() - i < count) return false;
        for (unsigned j = 0; j < count; ++j) {
            const auto byte = static_cast<unsigned char>(text[i++]);
            if ((byte & 0xC0U) != 0x80U) return false;
            value = (value << 6U) | (byte & 0x3FU);
        }
        if (value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) return false;
    }
    return true;
}

Status IpAddress::parse(std::string_view text, IpAddress& out) noexcept {
    if (text.empty() || text.size() >= INET6_ADDRSTRLEN) return Status::InvalidIp;
    for (const char character : text) {
        const auto c = static_cast<unsigned char>(character);
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F') || c == ':' || c == '.')) return Status::InvalidIp;
    }
    char numeric[INET6_ADDRSTRLEN]{};
    std::memcpy(numeric, text.data(), text.size());
    IpAddress parsed;
    parsed.v6 = text.find(':') != std::string_view::npos;
    if (inet_pton(parsed.v6 ? AF_INET6 : AF_INET, numeric, parsed.bytes.data()) != 1) return Status::InvalidIp;
    out = parsed;
    return Status::Ok;
}

Status Path::parse(std::string_view text) noexcept {
    parts.fill(nullptr);
    if (text.size() > 1024 || !valid_utf8(text) || (!text.empty() && text.front() != '/')) return Status::InvalidPath;
    if (text.empty()) return Status::Ok;
    std::size_t token = 0, write = 0;
    parts[token++] = storage.data();
    for (std::size_t i = 1; i < text.size(); ++i) {
        char c = text[i];
        if (c == '/') {
            if (token == 32) { parts.fill(nullptr); return Status::InvalidPath; }
            storage[write++] = '\0';
            parts[token++] = storage.data() + write;
            continue;
        }
        if (c == '~') {
            if (++i == text.size() || (text[i] != '0' && text[i] != '1')) { parts.fill(nullptr); return Status::InvalidPath; }
            c = text[i] == '0' ? '~' : '/';
        }
        storage[write++] = c;
    }
    storage[write] = '\0';
    return Status::Ok;
}

Database::~Database() { if (opened_) MMDB_close(&db_); }
Status Database::open(std::string_view path, std::shared_ptr<const Database>& out) {
    if (path.empty() || path.size() > 4096 || !valid_utf8(path)) return Status::InvalidArgument;
    const std::string filename(path);
    std::error_code ec;
    const auto file = std::filesystem::u8path(filename);
    if (!std::filesystem::is_regular_file(file, ec) || ec) return Status::OpenFailed;
    const auto size = std::filesystem::file_size(file, ec);
    if (ec) return Status::OpenFailed;
    if (size == 0 || size > (std::uintmax_t{1} << 30U)) return Status::InvalidDatabase;
    auto database = std::shared_ptr<Database>(new Database);
    const int error = MMDB_open(filename.c_str(), MMDB_MODE_MMAP, &database->db_);
    if (error != MMDB_SUCCESS) {
        if (error == MMDB_FILE_OPEN_ERROR || error == MMDB_IO_ERROR) return Status::OpenFailed;
        if (error == MMDB_UNKNOWN_DATABASE_FORMAT_ERROR) return Status::UnsupportedDatabase;
        if (error == MMDB_OUT_OF_MEMORY_ERROR) return Status::ResourceLimit;
        return Status::InvalidDatabase;
    }
    database->opened_ = true;
    const auto& meta = database->db_.metadata;
    if (meta.binary_format_major_version != 2 || (meta.ip_version != 4 && meta.ip_version != 6) ||
        (meta.record_size != 24 && meta.record_size != 28 && meta.record_size != 32) ||
        !meta.database_type || !valid_utf8(meta.database_type)) return Status::InvalidDatabase;
    out = std::move(database);
    return Status::Ok;
}

Status Value::string(std::string_view& out) const noexcept {
    if (data.type != MMDB_DATA_TYPE_UTF8_STRING) return Status::TypeMismatch;
    const std::string_view value(data.utf8_string, data.data_size);
    if (!valid_utf8(value)) return Status::InvalidUtf8;
    out = value;
    return Status::Ok;
}
Status Value::integer(std::int32_t& out) const noexcept {
    std::uint64_t value = 0;
    switch (data.type) {
        case MMDB_DATA_TYPE_INT32: out = data.int32; return Status::Ok;
        case MMDB_DATA_TYPE_UINT16: value = data.uint16; break;
        case MMDB_DATA_TYPE_UINT32: value = data.uint32; break;
        case MMDB_DATA_TYPE_UINT64: value = data.uint64; break;
        case MMDB_DATA_TYPE_UINT128: {
#if MMDB_UINT128_IS_BYTE_ARRAY
            for (unsigned i = 0; i < 12; ++i) if (data.uint128[i]) return Status::OutOfRange;
            for (unsigned i = 12; i < 16; ++i) value = (value << 8U) | data.uint128[i];
#else
            if (data.uint128 > static_cast<decltype(data.uint128)>(INT32_MAX)) return Status::OutOfRange;
            value = static_cast<std::uint64_t>(data.uint128);
#endif
            break;
        }
        default: return Status::TypeMismatch;
    }
    if (value > static_cast<std::uint64_t>(INT32_MAX)) return Status::OutOfRange;
    out = static_cast<std::int32_t>(value);
    return Status::Ok;
}
Status Value::floating(float& out) const noexcept {
    double value;
    if (data.type == MMDB_DATA_TYPE_FLOAT) value = data.float_value;
    else if (data.type == MMDB_DATA_TYPE_DOUBLE) value = data.double_value;
    else return Status::TypeMismatch;
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max() ||
        (value != 0 && std::abs(value) < std::numeric_limits<float>::denorm_min())) return Status::OutOfRange;
    out = static_cast<float>(value);
    return Status::Ok;
}
Status Value::boolean(bool& out) const noexcept {
    if (data.type != MMDB_DATA_TYPE_BOOLEAN) return Status::TypeMismatch;
    out = data.boolean;
    return Status::Ok;
}
Status Value::text(std::string& out) const {
    switch (data.type) {
        case MMDB_DATA_TYPE_INT32: out = std::to_string(data.int32); return Status::Ok;
        case MMDB_DATA_TYPE_UINT16: out = std::to_string(data.uint16); return Status::Ok;
        case MMDB_DATA_TYPE_UINT32: out = std::to_string(data.uint32); return Status::Ok;
        case MMDB_DATA_TYPE_UINT64: out = std::to_string(data.uint64); return Status::Ok;
        case MMDB_DATA_TYPE_UINT128: {
            std::array<unsigned char, 16> bytes{};
#if MMDB_UINT128_IS_BYTE_ARRAY
            std::copy_n(data.uint128, 16, bytes.begin());
#else
            auto value = data.uint128;
            for (unsigned i = 0; i < 16; ++i) { bytes[15U - i] = static_cast<unsigned char>(value & 255U); value >>= 8U; }
#endif
            std::string decimal;
            bool nonzero;
            do {
                unsigned remainder = 0;
                nonzero = false;
                for (auto& b : bytes) {
                    const unsigned current = remainder * 256U + b;
                    b = static_cast<unsigned char>(current / 10U);
                    remainder = current % 10U;
                    nonzero = nonzero || b != 0;
                }
                decimal.push_back(static_cast<char>('0' + remainder));
            } while (nonzero);
            std::reverse(decimal.begin(), decimal.end());
            out = std::move(decimal);
            return Status::Ok;
        }
        case MMDB_DATA_TYPE_BYTES: {
            // Bounded by the public Pawn output limit. Avoid expanding huge values.
            if (data.data_size > 32767) return Status::OutOfRange;
            static constexpr char hex[] = "0123456789abcdef";
            std::string value(static_cast<std::size_t>(data.data_size) * 2U, '\0');
            for (std::size_t i = 0; i < data.data_size; ++i) {
                value[i * 2] = hex[data.bytes[i] >> 4U];
                value[i * 2 + 1] = hex[data.bytes[i] & 15U];
            }
            out = std::move(value); return Status::Ok;
        }
        default: return Status::TypeMismatch;
    }
}

Status Lookup::get(std::string_view path, Value& out) const noexcept {
    Path parsed;
    auto status = parsed.parse(path);
    if (status != Status::Ok) return status;
    MMDB_entry_data_s value{};
    auto start = entry;
    const int error = MMDB_aget_value(&start, &value, parsed.parts.data());
    if (error == MMDB_LOOKUP_PATH_DOES_NOT_MATCH_DATA_ERROR) return Status::NoField;
    if (error == MMDB_INVALID_LOOKUP_PATH_ERROR) return Status::InvalidPath;
    if (error != MMDB_SUCCESS) return Status::DecoderError;
    if (!value.has_data) return Status::NoField;
    switch (value.type) {
        case MMDB_DATA_TYPE_UTF8_STRING: case MMDB_DATA_TYPE_DOUBLE: case MMDB_DATA_TYPE_BYTES:
        case MMDB_DATA_TYPE_UINT16: case MMDB_DATA_TYPE_UINT32: case MMDB_DATA_TYPE_MAP:
        case MMDB_DATA_TYPE_INT32: case MMDB_DATA_TYPE_UINT64: case MMDB_DATA_TYPE_UINT128:
        case MMDB_DATA_TYPE_ARRAY: case MMDB_DATA_TYPE_BOOLEAN: case MMDB_DATA_TYPE_FLOAT: break;
        default: return Status::DecoderError;
    }
    out.data = value;
    return Status::Ok;
}

Status Service::find(Owner owner, Handle handle, const Slot*& out) const noexcept {
    if (handle <= 0) return Status::InvalidHandle;
    const auto value = static_cast<std::uint32_t>(handle);
    const auto index = value & 4095U;
    if (!index || index > slots_.size()) return Status::InvalidHandle;
    const auto& slot = slots_[index - 1U];
    if (slot.generation != (value >> 12U) || slot.resource.index() == 0) return Status::InvalidHandle;
    if (slot.owner != owner) return Status::WrongOwner;
    out = &slot;
    return Status::Ok;
}
std::size_t Service::count(Owner owner, std::size_t kind) const noexcept {
    return static_cast<std::size_t>(std::count_if(slots_.begin(), slots_.end(), [=](const Slot& s) {
        return s.owner == owner && s.resource.index() == kind;
    }));
}
Status Service::insert(Owner owner, Resource resource, Handle& out) {
    if (!owner) return Status::InvalidArgument;
    if (count(owner, resource.index()) >= (resource.index() == 1 ? max_databases_per_owner : max_lookups_per_owner)) return Status::ResourceLimit;
    auto it = std::find_if(slots_.begin(), slots_.end(), [](const Slot& slot) { return slot.generation && slot.resource.index() == 0; });
    if (it == slots_.end()) {
        if (slots_.size() == max_resources) return Status::ResourceLimit;
        slots_.emplace_back();
        it = slots_.end() - 1;
    }
    const auto index = static_cast<std::uint32_t>(it - slots_.begin()) + 1U;
    it->resource = std::move(resource);
    it->owner = owner;
    out = static_cast<Handle>((it->generation << 12U) | index);
    return Status::Ok;
}
void Service::invalidate(Slot& slot) noexcept {
    slot.resource = std::monostate{};
    slot.owner = 0;
    slot.generation = slot.generation == 524287 ? 0 : slot.generation + 1U;
}
Status Service::load(Owner owner, std::string_view path, Handle& out) {
    if (!owner) return Status::InvalidArgument;
    if (count(owner, 1) >= max_databases_per_owner) return Status::ResourceLimit;
    std::shared_ptr<const Database> db;
    auto status = Database::open(path, db);
    return status == Status::Ok ? insert(owner, std::move(db), out) : status;
}
Status Service::reload(Owner owner, Handle handle, std::string_view path) {
    const Slot* slot = nullptr;
    auto status = find(owner, handle, slot);
    if (status != Status::Ok) return status;
    if (slot->resource.index() != 1) return Status::InvalidHandle;
    std::shared_ptr<const Database> replacement;
    status = Database::open(path, replacement);
    if (status == Status::Ok) const_cast<Slot*>(slot)->resource = std::move(replacement);
    return status;
}
Status Service::unload(Owner owner, Handle handle) noexcept {
    const Slot* slot = nullptr;
    auto status = find(owner, handle, slot);
    if (status != Status::Ok) return status;
    if (slot->resource.index() != 1) return Status::InvalidHandle;
    invalidate(*const_cast<Slot*>(slot));
    return Status::Ok;
}
Status Service::valid(Owner owner, Handle handle, bool& out) const noexcept {
    const Slot* slot = nullptr;
    const auto status = find(owner, handle, slot);
    if (status == Status::WrongOwner) return status;
    out = status == Status::Ok && slot->resource.index() == 1;
    return Status::Ok;
}
Status Service::lookup(Owner owner, Handle handle, std::string_view ip, Handle& out) {
    const Slot* slot = nullptr;
    auto status = find(owner, handle, slot);
    if (status != Status::Ok) return status;
    const auto* database = std::get_if<std::shared_ptr<const Database>>(&slot->resource);
    if (!database) return Status::InvalidHandle;
    IpAddress address;
    status = IpAddress::parse(ip, address);
    if (status != Status::Ok) return status;
    sockaddr_storage storage{};
    if (address.v6) {
        auto* v6 = reinterpret_cast<sockaddr_in6*>(&storage);
        v6->sin6_family = AF_INET6;
        std::memcpy(&v6->sin6_addr, address.bytes.data(), 16);
    } else {
        auto* v4 = reinterpret_cast<sockaddr_in*>(&storage);
        v4->sin_family = AF_INET;
        std::memcpy(&v4->sin_addr, address.bytes.data(), 4);
    }
    int error = MMDB_SUCCESS;
    const auto found = MMDB_lookup_sockaddr(&(*database)->raw(), reinterpret_cast<const sockaddr*>(&storage), &error);
    if (error == MMDB_IPV6_LOOKUP_IN_IPV4_DATABASE_ERROR) return Status::UnsupportedFamily;
    if (error != MMDB_SUCCESS) return Status::DecoderError;
    if (!found.found_entry) return Status::NoRecord;
    auto prefix = found.netmask;
    if (!address.v6 && (*database)->raw().metadata.ip_version == 6) {
        if (prefix < 96) return Status::DecoderError;
        prefix = static_cast<std::uint16_t>(prefix - 96U);
    }
    if (prefix > (address.v6 ? 128 : 32)) return Status::DecoderError;
    Lookup result{*database, found.entry, prefix};
    return insert(owner, std::move(result), out);
}
Status Service::result(Owner owner, Handle handle, const Lookup*& out) const noexcept {
    const Slot* slot = nullptr;
    const auto status = find(owner, handle, slot);
    if (status != Status::Ok) return status;
    const auto* result = std::get_if<Lookup>(&slot->resource);
    if (!result) return Status::InvalidHandle;
    out = result;
    return Status::Ok;
}
Status Service::release(Owner owner, Handle handle) noexcept {
    const Slot* slot = nullptr;
    const auto status = find(owner, handle, slot);
    if (status != Status::Ok) return status;
    if (slot->resource.index() != 2) return Status::InvalidHandle;
    invalidate(*const_cast<Slot*>(slot));
    return Status::Ok;
}
Status Service::metadata(Owner owner, Handle handle, std::string& type, std::string& epoch, std::int32_t& ip_version) const {
    const Slot* slot = nullptr;
    const auto status = find(owner, handle, slot);
    if (status != Status::Ok) return status;
    const auto* database = std::get_if<std::shared_ptr<const Database>>(&slot->resource);
    if (!database) return Status::InvalidHandle;
    const auto& meta = (*database)->raw().metadata;
    type = meta.database_type;
    epoch = std::to_string(meta.build_epoch);
    ip_version = meta.ip_version;
    return Status::Ok;
}
void Service::clear_owner(Owner owner) noexcept {
    for (auto& slot : slots_) if (slot.owner == owner && slot.resource.index()) invalidate(slot);
}
void Service::clear() noexcept {
    for (auto& slot : slots_) if (slot.resource.index()) invalidate(slot);
}
std::size_t Service::size() const noexcept {
    return static_cast<std::size_t>(std::count_if(slots_.begin(), slots_.end(), [](const Slot& s) { return s.resource.index() != 0; }));
}
} // namespace maxora
