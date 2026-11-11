#include "core.hpp"
#include <algorithm>
#include <charconv>
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

namespace mmdb {
const char* status_message(Status status) noexcept {
	static constexpr const char* messages[] = {
		"Success", "Invalid argument or AMX memory", "Database is not loaded",
		"Database name is already loaded", "Invalid numeric IP address", "Invalid field path",
		"Cannot open database file", "Invalid database", "Unsupported database format",
		"No record for address", "Field does not exist", "Value has a different type",
		"Value is outside the destination range", "Destination buffer is too small",
		"Invalid UTF-8 or embedded NUL", "Resource limit reached", "Database decoder error",
		"Internal failure", "IPv6 lookup in IPv4 database"
	};
	static_assert(sizeof(messages) / sizeof(messages[0]) == static_cast<std::size_t>(Status::UnsupportedFamily) + 1);
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

namespace {
bool validate_sections(MMDB_s& db) noexcept {
	constexpr std::uint64_t separator_size = 16;
	constexpr std::uint64_t metadata_marker_size = 14;
	const auto tree_size = static_cast<std::uint64_t>(db.metadata.node_count) * db.full_record_byte_size;
	const auto data_offset = tree_size + separator_size;
	const auto file_size = static_cast<std::uint64_t>(db.file_size);
	const auto metadata_size = metadata_marker_size + db.metadata_section_size;
	if (file_size < metadata_size || data_offset > file_size - metadata_size) return false;
	if (!std::all_of(db.file_content + tree_size, db.file_content + data_offset,
			[](std::uint8_t byte) { return byte == 0; })) return false;

	// The reader's mapped length includes metadata. Keep record decoding inside
	// the actual data section so corrupt pointers cannot return metadata as data.
	db.data_section_size = static_cast<std::uint32_t>(file_size - metadata_size - data_offset);
	return true;
}

bool validate_database_type(const MMDB_s& db) noexcept {
	// Decode with the original length to catch embedded NULs in the metadata.
	// libmaxminddb exposes a copied, NUL-terminated database type to consumers.
	MMDB_s metadata{};
	metadata.data_section = db.metadata_section;
	metadata.data_section_size = db.metadata_section_size;
	MMDB_entry_s root{&metadata, 0};
	MMDB_entry_data_s type{};
	const char* path[] = {"database_type", nullptr};
	return db.metadata.database_type && MMDB_aget_value(&root, &type, path) == MMDB_SUCCESS &&
		type.type == MMDB_DATA_TYPE_UTF8_STRING && valid_utf8({type.utf8_string, type.data_size});
}
} // namespace

Database::~Database() {
	if (opened_) MMDB_close(&db_);
}

Status Database::open(std::string_view path, std::shared_ptr<const Database>& out) {
	if (path.empty() || path.size() > 4096 || !valid_utf8(path)) return Status::InvalidArgument;
	const std::string filename(path);
	std::error_code ec;
#ifdef _WIN32
	const std::filesystem::path file{std::u8string(path.begin(), path.end())};
#else
	const std::filesystem::path file{filename};
#endif
	if (!std::filesystem::is_regular_file(file, ec) || ec) return Status::OpenFailed;
	const auto size = std::filesystem::file_size(file, ec);
	if (ec) return Status::OpenFailed;
	if (size == 0) return Status::InvalidDatabase;
	// libmaxminddb maps the whole file and stores the length in ssize_t.
	constexpr auto mapping_limit = std::numeric_limits<decltype(MMDB_s::file_size)>::max();
	if (size > static_cast<std::uintmax_t>(mapping_limit)) return Status::OutOfRange;
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
		!validate_sections(database->db_) || !validate_database_type(database->db_)) return Status::InvalidDatabase;
	out = std::move(database);
	return Status::Ok;
}

Status Value::string(std::string_view& out) const noexcept {
	if (data.type != MMDB_DATA_TYPE_UTF8_STRING) return Status::TypeMismatch;
	if (data.data_size > 1048575) return Status::OutOfRange;
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
			// Bound transient allocation and hexadecimal expansion on the Pawn thread.
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

namespace {
Status read_entry(MMDB_entry_s entry, const char* token, MMDB_entry_data_s& value) noexcept {
	const char* path[] = {token, nullptr};
	const int error = MMDB_aget_value(&entry, &value, path);
	if (error == MMDB_LOOKUP_PATH_DOES_NOT_MATCH_DATA_ERROR) return Status::NoField;
	if (error != MMDB_SUCCESS || !value.has_data) return Status::DecoderError;
	return Status::Ok;
}

Status check_index(std::string_view token, std::uint32_t size) noexcept {
	if (token.empty() || (token.front() == '0' && token.size() > 1) ||
		token.find_first_not_of("0123456789") != std::string_view::npos) return Status::InvalidPath;

	std::uint32_t index = 0;
	const auto result = std::from_chars(token.data(), token.data() + token.size(), index);
	return result.ec == std::errc{} && index < size ? Status::Ok : Status::NoField;
}
} // namespace

Status Lookup::get(std::string_view path, Value& out) const noexcept {
	Path parsed;
	const auto status = parsed.parse(path);
	return status == Status::Ok ? get(parsed, out) : status;
}

Status Lookup::get(const Path& path, Value& out) const noexcept {
	if (!database_) return Status::InvalidArgument;
	MMDB_entry_data_s value{};
	auto status = read_entry(entry_, nullptr, value);
	if (status != Status::Ok) return status;
	for (const char* token : path.parts) {
		if (!token) break;
		if (value.type == MMDB_DATA_TYPE_ARRAY) {
			status = check_index(token, value.data_size);
			if (status != Status::Ok) return status;
		} else if (value.type != MMDB_DATA_TYPE_MAP) {
			return Status::NoField;
		}
		// Use the resolved offset so arrays and maps stored behind a pointer
		// can be traversed one token at a time with type-aware index checking.
		status = read_entry({entry_.mmdb, value.offset}, token, value);
		if (status != Status::Ok) return status;
	}

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

Status Lookup::find(std::shared_ptr<const Database> database, std::string_view ip, Lookup& out) noexcept {
	if (!database) return Status::InvalidArgument;
	IpAddress address;
	const auto status = IpAddress::parse(ip, address);
	if (status != Status::Ok) return status;

	const auto& db = database->raw();
	int error = MMDB_SUCCESS;
	MMDB_lookup_result_s found;
	if (address.v6) {
		sockaddr_in6 v6{};
		v6.sin6_family = AF_INET6;
		std::memcpy(&v6.sin6_addr, address.bytes.data(), address.bytes.size());
		found = MMDB_lookup_sockaddr(&db, reinterpret_cast<const sockaddr*>(&v6), &error);
	} else {
		sockaddr_in v4{};
		v4.sin_family = AF_INET;
		std::memcpy(&v4.sin_addr, address.bytes.data(), sizeof(v4.sin_addr));
		found = MMDB_lookup_sockaddr(&db, reinterpret_cast<const sockaddr*>(&v4), &error);
	}
	if (error == MMDB_IPV6_LOOKUP_IN_IPV4_DATABASE_ERROR) return Status::UnsupportedFamily;
	if (error != MMDB_SUCCESS) return Status::DecoderError;
	if (!found.found_entry) return Status::NoRecord;
	if (found.entry.offset >= db.data_section_size) return Status::DecoderError;

	auto prefix = found.netmask;
	if (!address.v6 && db.metadata.ip_version == 6) {
		// A record before bit 96 covers the whole embedded IPv4 space.
		prefix = prefix <= 96 ? 0 : static_cast<std::uint16_t>(prefix - 96U);
	}
	if (prefix > (address.v6 ? 128 : 32)) return Status::DecoderError;
	out.database_ = std::move(database);
	out.entry_ = found.entry;
	out.prefix = prefix;
	return Status::Ok;
}
} // namespace mmdb
