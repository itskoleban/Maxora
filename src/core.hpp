#pragma once

#include <maxminddb.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace mmdb {
enum class Status : std::int32_t {
	Ok = 0, InvalidArgument, NotLoaded, AlreadyLoaded, InvalidIp, InvalidPath,
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

// An empty JSON Pointer selects the root. Escapes ~0 and ~1 decode to ~ and /.
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

// A call-local record keeps its immutable mapping alive across a replacement.
// Values returned by get() borrow from this mapping until the Lookup expires.
class Lookup final {
public:
	static Status find(std::shared_ptr<const Database> database, std::string_view ip, Lookup& out) noexcept;
	Status get(std::string_view path, Value& out) const noexcept;
	Status get(const Path& path, Value& out) const noexcept;
	std::uint16_t prefix = 0;

private:
	std::shared_ptr<const Database> database_;
	MMDB_entry_s entry_{};
};
} // namespace mmdb
