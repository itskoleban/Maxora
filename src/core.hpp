#pragma once

#include <maxminddb.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace maxora {
using Handle = std::int32_t;
using Owner = std::uint64_t;
enum class Status : std::int32_t {
    Ok = 0, InvalidArgument, InvalidHandle, WrongOwner, InvalidIp, InvalidPath,
    OpenFailed, InvalidDatabase, UnsupportedDatabase, NoRecord, NoField,
    TypeMismatch, OutOfRange, BufferTooSmall, InvalidUtf8, ResourceLimit,
    DecoderError, InternalError, UnsupportedFamily
};
const char* status_message(Status status) noexcept;
bool valid_utf8(std::string_view text) noexcept;
struct IpAddress {
    std::array<unsigned char, 16> bytes{};
    bool v6 = false;
    static Status parse(std::string_view text, IpAddress& out) noexcept;
};
// JSON Pointer syntax: empty = root, /key/0, ~0 = ~, ~1 = /.
// The bounded parser supports empty map keys and at most 32 path tokens.
struct Path {
    std::array<char, 1025> storage{};
    std::array<const char*, 33> parts{};
    Path() = default;
    Path(const Path&) = delete;
    Path& operator=(const Path&) = delete;
    Status parse(std::string_view text) noexcept;
};
class Database final {
public:
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    static Status open(std::string_view path, std::shared_ptr<const Database>& out);
    const MMDB_s& raw() const noexcept { return db_; }
private:
    Database() = default;
    MMDB_s db_{};
    bool opened_ = false;
};
struct Value {
    MMDB_entry_data_s data{};
    Status string(std::string_view& out) const noexcept;
    Status integer(std::int32_t& out) const noexcept;
    Status floating(float& out) const noexcept;
    Status boolean(bool& out) const noexcept;
    // Decimal for all integer types, lowercase hexadecimal for bytes.
    Status text(std::string& out) const;
};
struct Lookup {
    std::shared_ptr<const Database> database;
    MMDB_entry_s entry{};
    std::uint16_t prefix = 0;
    Status get(std::string_view path, Value& out) const noexcept;
};
// A synchronous, main-thread service. No concurrent callers are supported.
// A single typed resource table prevents DB/result handles aliasing each other.
class Service final {
public:
    static constexpr std::size_t max_resources = 4095;
    static constexpr std::size_t max_databases_per_owner = 64;
    static constexpr std::size_t max_lookups_per_owner = 256;
    Status load(Owner owner, std::string_view path, Handle& out);
    Status reload(Owner owner, Handle handle, std::string_view path);
    Status unload(Owner owner, Handle handle) noexcept;
    Status valid(Owner owner, Handle handle, bool& out) const noexcept;
    Status lookup(Owner owner, Handle database, std::string_view ip, Handle& out);
    Status release(Owner owner, Handle lookup) noexcept;
    // Internal borrowed pointer: valid only until the next service mutation.
    Status result(Owner owner, Handle handle, const Lookup*& out) const noexcept;
    Status metadata(Owner owner, Handle database, std::string& type,
                    std::string& epoch, std::int32_t& ip_version) const;
    void clear_owner(Owner owner) noexcept;
    void clear() noexcept;
    std::size_t size() const noexcept;
private:
    using Resource = std::variant<std::monostate, std::shared_ptr<const Database>, Lookup>;
    struct Slot { std::uint32_t generation = 1; Owner owner = 0; Resource resource; };
    std::vector<Slot> slots_;
    Status find(Owner owner, Handle handle, const Slot*& out) const noexcept;
    Status insert(Owner owner, Resource resource, Handle& out);
    void invalidate(Slot& slot) noexcept;
    std::size_t count(Owner owner, std::size_t variant_index) const noexcept;
};
} // namespace maxora
