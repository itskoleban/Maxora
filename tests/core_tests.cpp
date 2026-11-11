#include "core.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <iterator>

#ifndef MMDB_FIXTURE_DIR
#define MMDB_FIXTURE_DIR "lib/libmaxminddb/t/maxmind-db/test-data"
#endif

namespace {
using namespace mmdb;
std::size_t checks = 0;
std::filesystem::path fixtures = MMDB_FIXTURE_DIR;

void check(bool condition, const char* expression, int line) {
	++checks;
	if (!condition) throw std::runtime_error(std::string("line ") + std::to_string(line) + ": " + expression);
}
#define CHECK(expression) check((expression), #expression, __LINE__)
#define EXPECT(expression, status) CHECK((expression) == Status::status)

std::string utf8_path(const std::filesystem::path& path) {
	const auto bytes = path.u8string();
	return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

std::string fixture(const char* name) {
	return utf8_path(fixtures / name);
}

std::shared_ptr<const Database> open(const char* name) {
	std::shared_ptr<const Database> database;
	EXPECT(Database::open(fixture(name), database), Ok);
	CHECK(database != nullptr);
	return database;
}

Lookup lookup(const std::shared_ptr<const Database>& database, const char* ip) {
	Lookup found;
	EXPECT(Lookup::find(database, ip, found), Ok);
	return found;
}

Value field(const Lookup& lookup_result, const char* path) {
	Value value;
	EXPECT(lookup_result.get(path, value), Ok);
	CHECK(value.data.has_data);
	return value;
}

void test_ip() {
	for (const char* text : {"0.0.0.0", "255.255.255.255", "127.0.0.1", "1.2.3.4"}) {
		IpAddress ip;
		EXPECT(IpAddress::parse(text, ip), Ok);
		CHECK(!ip.v6);
	}
	IpAddress ip;
	EXPECT(IpAddress::parse("1.2.3.4", ip), Ok);
	CHECK(ip.bytes[0] == 1 && ip.bytes[1] == 2 && ip.bytes[2] == 3 && ip.bytes[3] == 4);
	for (const char* text : {"::", "::1", "2001:db8::1234", "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", "::ffff:192.0.2.1"}) {
		EXPECT(IpAddress::parse(text, ip), Ok);
		CHECK(ip.v6);
	}
	for (const char* text : {"", "localhost", "example.com", "127.1", "1.2.3", "1.2.3.4.5", "256.1.1.1", "01.2.3.4", "0x7f000001", "2130706433", " 1.2.3.4", "1.2.3.4 ", "1.2.3.4:80", "1.2.3.4/32", "[::1]", "fe80::1%eth0", "fe80::1%1", "1:::2", "::g", "::1/128", "1:2:3:4:5:6:7:8:9"}) {
		EXPECT(IpAddress::parse(text, ip), InvalidIp);
	}
	EXPECT(IpAddress::parse(std::string_view("1.2.3.4\0x", 9), ip), InvalidIp);
	EXPECT(IpAddress::parse(std::string(4096, '1'), ip), InvalidIp);
}

void test_path_and_utf8() {
	Path path;
	EXPECT(path.parse(""), Ok);
	CHECK(path.parts[0] == nullptr);
	EXPECT(path.parse("/city/names/en"), Ok);
	CHECK(std::string_view(path.parts[0]) == "city");
	CHECK(std::string_view(path.parts[1]) == "names");
	CHECK(std::string_view(path.parts[2]) == "en");
	CHECK(path.parts[3] == nullptr);
	EXPECT(path.parse("/~0/~1/~01//"), Ok);
	CHECK(std::string_view(path.parts[0]) == "~");
	CHECK(std::string_view(path.parts[1]) == "/");
	CHECK(std::string_view(path.parts[2]) == "~1");
	CHECK(std::string_view(path.parts[3]).empty());
	CHECK(std::string_view(path.parts[4]).empty());
	EXPECT(path.parse("/"), Ok);
	CHECK(std::string_view(path.parts[0]).empty() && path.parts[1] == nullptr);
	for (const char* text : {"no-slash", "~", "/~", "/~2", "/x~x"}) EXPECT(path.parse(text), InvalidPath);
	EXPECT(path.parse(std::string_view("/a\0b", 4)), InvalidPath);
	EXPECT(path.parse(std::string(1025, '/')), InvalidPath);
	std::string boundary;
	for (int i = 0; i != 32; ++i) boundary += "/a";
	EXPECT(path.parse(boundary), Ok);
	CHECK(path.parts[32] == nullptr);
	EXPECT(path.parse(boundary + "/b"), InvalidPath);
	EXPECT(path.parse("/" + std::string(1023, 'a')), Ok);
	EXPECT(path.parse("/" + std::string(1024, 'a')), InvalidPath);

	CHECK(valid_utf8(""));
	CHECK(valid_utf8("plain ASCII"));
	CHECK(valid_utf8("\xc2\xa2\xe2\x98\xaf\xf0\x9f\x8c\x8d"));
	for (const std::string& text : {std::string("\x80"), std::string("\xc0\xaf"), std::string("\xe0\x80\xaf"), std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80"), std::string("\xf5\x80\x80\x80"), std::string("\xc2"), std::string("\xe2\x82"), std::string("\xf0\x9f\x8c"), std::string("\xc2\x20")}) CHECK(!valid_utf8(text));
}

void test_conversion() {
	Value value;
	value.data.has_data = true;
	std::int32_t integer = 0;
	float number = 0;
	bool flag = false;
	std::string text;
	std::string_view borrowed;
	value.data.type = MMDB_DATA_TYPE_INT32;
	for (const auto input : {std::numeric_limits<std::int32_t>::min(), -1, 0, std::numeric_limits<std::int32_t>::max()}) {
		value.data.int32 = input;
		EXPECT(value.integer(integer), Ok); CHECK(integer == input);
		EXPECT(value.text(text), Ok); CHECK(text == std::to_string(input));
	}
	value.data.type = MMDB_DATA_TYPE_UINT32;
	value.data.uint32 = 2147483647U;
	EXPECT(value.integer(integer), Ok); CHECK(integer == 2147483647);
	value.data.uint32 = 2147483648U;
	EXPECT(value.integer(integer), OutOfRange);
	value.data.uint32 = std::numeric_limits<std::uint32_t>::max();
	EXPECT(value.text(text), Ok); CHECK(text == "4294967295");
	value.data.type = MMDB_DATA_TYPE_UINT64;
	value.data.uint64 = std::numeric_limits<std::uint64_t>::max();
	EXPECT(value.integer(integer), OutOfRange);
	EXPECT(value.text(text), Ok); CHECK(text == "18446744073709551615");
	value.data.type = MMDB_DATA_TYPE_UINT16;
	value.data.uint16 = 65535;
	EXPECT(value.integer(integer), Ok); CHECK(integer == 65535);
	value.data.type = MMDB_DATA_TYPE_BOOLEAN;
	value.data.boolean = true;
	EXPECT(value.boolean(flag), Ok); CHECK(flag);
	EXPECT(value.integer(integer), TypeMismatch);
	EXPECT(value.string(borrowed), TypeMismatch);
	value.data.type = MMDB_DATA_TYPE_DOUBLE;
	value.data.double_value = 42.125;
	EXPECT(value.floating(number), Ok); CHECK(number == 42.125F);
	value.data.double_value = std::numeric_limits<double>::max();
	EXPECT(value.floating(number), OutOfRange);
	value.data.double_value = std::numeric_limits<double>::infinity();
	EXPECT(value.floating(number), OutOfRange);
	value.data.double_value = std::numeric_limits<double>::quiet_NaN();
	EXPECT(value.floating(number), OutOfRange);
	value.data.double_value = static_cast<double>(std::numeric_limits<float>::denorm_min()) / 2;
	EXPECT(value.floating(number), OutOfRange);
	value.data.double_value = static_cast<double>(std::numeric_limits<float>::max());
	EXPECT(value.floating(number), Ok); CHECK(number == std::numeric_limits<float>::max());
	value.data.type = MMDB_DATA_TYPE_FLOAT;
	value.data.float_value = -1.5F;
	EXPECT(value.floating(number), Ok); CHECK(number == -1.5F);
	value.data.type = MMDB_DATA_TYPE_UTF8_STRING;
	const char exact[] = {'a', 'b', 'c', 'x'};
	value.data.utf8_string = exact; value.data.data_size = 3;
	EXPECT(value.string(borrowed), Ok); CHECK(borrowed == "abc");
	value.data.utf8_string = "\xc0\xaf"; value.data.data_size = 2;
	EXPECT(value.string(borrowed), InvalidUtf8);
	value.data.utf8_string = "a\0b"; value.data.data_size = 3;
	EXPECT(value.string(borrowed), InvalidUtf8);
	const std::string maximum_text(1048575, 'a');
	value.data.utf8_string = maximum_text.data(); value.data.data_size = 1048575;
	EXPECT(value.string(borrowed), Ok); CHECK(borrowed.size() == maximum_text.size());
	// Reject before scanning: the backing object deliberately has one byte.
	value.data.utf8_string = "a"; value.data.data_size = 1048576;
	EXPECT(value.string(borrowed), OutOfRange);
	value.data.type = MMDB_DATA_TYPE_BYTES;
	const std::uint8_t bytes[] = {0, 0x7f, 0xff};
	value.data.bytes = bytes; value.data.data_size = 3;
	EXPECT(value.text(text), Ok); CHECK(text == "007fff");
	value.data.type = MMDB_DATA_TYPE_MAP;
	EXPECT(value.integer(integer), TypeMismatch);
	EXPECT(value.floating(number), TypeMismatch);
	EXPECT(value.boolean(flag), TypeMismatch);
	EXPECT(value.text(text), TypeMismatch);
	for (int i = 0; i <= static_cast<int>(Status::UnsupportedFamily); ++i) {
		CHECK(status_message(static_cast<Status>(i)) != nullptr);
		CHECK(std::string_view(status_message(static_cast<Status>(i))).size() > 0);
	}
}

void test_real_types() {
	const auto database = open("MaxMind-DB-test-decoder.mmdb");
	const auto found = lookup(database, "1.1.1.1");
	std::string_view string;
	auto value = field(found, "/utf8_string");
	EXPECT(value.string(string), Ok); CHECK(string == "unicode! \xe2\x98\xaf - \xe2\x99\xab");
	std::int32_t integer = 0;
	value = field(found, "/uint16"); EXPECT(value.integer(integer), Ok); CHECK(integer == 100);
	value = field(found, "/uint32"); EXPECT(value.integer(integer), Ok); CHECK(integer == 268435456);
	value = field(found, "/int32"); EXPECT(value.integer(integer), Ok); CHECK(integer == -268435456);
	value = field(found, "/uint64"); EXPECT(value.integer(integer), OutOfRange);
	std::string text;
	EXPECT(value.text(text), Ok); CHECK(text == "1152921504606846976");
	value = field(found, "/uint128"); EXPECT(value.text(text), Ok); CHECK(text == "1329227995784915872903807060280344576");
	value = field(found, "/bytes"); EXPECT(value.text(text), Ok); CHECK(text == "0000002a");
	value = field(found, "/boolean"); bool flag = false; EXPECT(value.boolean(flag), Ok); CHECK(flag);
	value = field(found, "/float"); float number = 0; EXPECT(value.floating(number), Ok); CHECK(std::fabs(number - 1.1F) < 0.0001F);
	value = field(found, "/double"); EXPECT(value.floating(number), Ok); CHECK(std::fabs(number - 42.123456F) < 0.00001F);
	value = field(found, "/array/2"); EXPECT(value.integer(integer), Ok); CHECK(integer == 3);
	Path parsed;
	EXPECT(parsed.parse("/array/2"), Ok);
	EXPECT(found.get(parsed, value), Ok);
	for (const char* path : {"/array/", "/array/-", "/array/-1", "/array/+1",
							"/array/00", "/array/ 1", "/array/not-a-number"}) {
		EXPECT(found.get(path, value), InvalidPath);
		EXPECT(value.integer(integer), Ok);
		CHECK(integer == 3);
	}
	EXPECT(found.get("/does-not-exist", value), NoField);
	EXPECT(found.get("/01", value), NoField);
	EXPECT(found.get("/array/999", value), NoField);
	EXPECT(found.get("/array/99999999999999999999", value), NoField);
	EXPECT(found.get("/uint16/nested", value), NoField);
	EXPECT(found.get("bad", value), InvalidPath);
	value = field(found, ""); CHECK(value.data.type == MMDB_DATA_TYPE_MAP);
	EXPECT(value.string(string), TypeMismatch);
	const auto nested_db = open("MaxMind-DB-test-nested.mmdb");
	const auto nested = lookup(nested_db, "1.1.1.1");
	CHECK(field(nested, "/map1/map2/array").data.data_size > 0);
	value = field(nested, "/map1/map2/array/0/map3/c");
	EXPECT(value.integer(integer), Ok); CHECK(integer == 3);
}

void test_ip_databases() {
	// A valid IPv6 record can cover embedded IPv4 at a prefix shorter than 96.
	const auto broad = open("MaxMind-DB-no-ipv4-search-tree.mmdb");
	for (const char* ip : {"1.1.1.1", "255.255.255.255"}) {
		const auto found = lookup(broad, ip);
		CHECK(found.prefix == 0);
		std::string_view text;
		EXPECT(field(found, "").string(text), Ok);
		CHECK(!text.empty());
	}
	for (const char* size : {"24", "28", "32"}) {
		for (const char* family : {"ipv4", "mixed"}) {
			const std::string name = std::string("MaxMind-DB-test-") + family + "-" + size + ".mmdb";
			const auto database = open(name.c_str());
			const auto found = lookup(database, "1.1.1.3");
			CHECK(found.prefix == 31);
			std::string_view text;
			EXPECT(field(found, "/ip").string(text), Ok);
			const bool ipv4 = std::string_view(family) == "ipv4";
			CHECK(text == (ipv4 ? "1.1.1.2" : "::1.1.1.2"));
			Lookup missing;
			EXPECT(Lookup::find(database, "2.3.4.5", missing), NoRecord);
			if (ipv4) EXPECT(Lookup::find(database, "::1", missing), UnsupportedFamily);
		}
		const std::string name = std::string("MaxMind-DB-test-ipv6-") + size + ".mmdb";
		const auto database = open(name.c_str());
		const auto found = lookup(database, "::2:0:1a");
		CHECK(found.prefix == 122);
		std::string_view text;
		EXPECT(field(found, "/ip").string(text), Ok);
		CHECK(text == "::2:0:0");
		Lookup missing;
		EXPECT(Lookup::find(database, "::abcd", missing), NoRecord);
	}
}

void test_snapshot_and_sustained_queries() {
	Lookup empty;
	Value value;
	EXPECT(empty.get("", value), InvalidArgument);

	auto database = open("MaxMind-DB-test-ipv4-24.mmdb");
	const auto found = lookup(database, "1.1.1.1");
	const std::weak_ptr<const Database> original = database;
	EXPECT(Database::open(fixture("missing.mmdb"), database), OpenFailed);
	CHECK(database == original.lock());
	EXPECT(Database::open(fixture("MaxMind-DB-test-decoder.mmdb"), database), Ok);
	CHECK(database != original.lock());
	std::string_view text;
	EXPECT(field(found, "/ip").string(text), Ok);
	CHECK(text == "1.1.1.1");

	const auto& meta = database->raw().metadata;
	CHECK(!std::string_view(meta.database_type).empty());
	CHECK(meta.build_epoch > 0 && meta.ip_version == 6);
	EXPECT(Lookup::find(database, "invalid", empty), InvalidIp);
	EXPECT(empty.get("", value), InvalidArgument);

	// Exceed the original per-slot generation limit using ordinary lookups.
	constexpr std::uint32_t repetitions = 600000;
	for (std::uint32_t i = 0; i < repetitions; ++i) {
		const auto query = lookup(database, "1.1.1.1");
		CHECK(field(query, "/uint16").data.uint16 == 100);
	}
	CHECK(database.use_count() == 1);
}

void test_bad_databases() {
	std::shared_ptr<const Database> database;
	EXPECT(Database::open("", database), InvalidArgument);
	EXPECT(Database::open(std::string_view("file\0.mmdb", 10), database), InvalidArgument);
	EXPECT(Database::open(fixture("not-present.mmdb"), database), OpenFailed);
	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	std::filesystem::path basename(u8"omp-mmdb-test-\u00e9-");
	basename += std::to_string(stamp) + ".mmdb";
	const auto temporary = std::filesystem::temp_directory_path() / basename;
	struct Cleanup {
		std::filesystem::path path;
		~Cleanup() {
			std::error_code error;
			std::filesystem::remove(path, error);
		}
	} cleanup{temporary};
	const auto filename = utf8_path(temporary);
	auto write = [&](std::string_view content) {
		std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
		output.write(content.data(), static_cast<std::streamsize>(content.size()));
		CHECK(output.good());
	};
	for (const auto& content : {std::string(), std::string("invalid MMDB"),
							   std::string("\xab\xcd\xefMaxMind.com", 14)}) {
		write(content);
		CHECK(Database::open(filename, database) != Status::Ok);
		CHECK(!database);
	}

	constexpr char source[] = "MaxMind-DB-test-ipv4-24.mmdb";
	const auto valid = open(source);
	const auto separator = static_cast<std::size_t>(valid->raw().metadata.node_count) *
		valid->raw().full_record_byte_size;
	std::ifstream input(fixtures / source, std::ios::binary);
	std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	CHECK(separator < bytes.size() && bytes[separator] == 0);
	write(bytes);
	EXPECT(Database::open(filename, database), Ok);
	database.reset();
	bytes[separator] = 1;
	write(bytes);
	EXPECT(Database::open(filename, database), InvalidDatabase);
	bytes[separator] = 0;
	const auto metadata = bytes.size() - valid->raw().metadata_section_size;
	const auto type = bytes.find(valid->raw().metadata.database_type, metadata);
	CHECK(type != std::string::npos);
	bytes[type] = '\0';
	write(bytes);
	EXPECT(Database::open(filename, database), InvalidDatabase);

	// An open validates the format and section boundaries. Record decoding is lazy.
	const auto bad_pointer = open("MaxMind-DB-test-broken-pointers-24.mmdb");
	const auto corrupt = lookup(bad_pointer, "1.1.1.16");
	Value value;
	EXPECT(corrupt.get("", value), DecoderError);
	EXPECT(corrupt.get("/ip", value), DecoderError);
	Lookup missing;
	EXPECT(Lookup::find(bad_pointer, "1.1.1.32", missing), DecoderError);
	const auto bad_tree = open("MaxMind-DB-test-broken-search-tree-24.mmdb");
	EXPECT(Lookup::find(bad_tree, "255.255.255.255", missing), DecoderError);
}
} // namespace

int main(int argc, char** argv) {
	if (argc == 3 && std::string_view(argv[1]) == "--fixtures") fixtures = argv[2];
	else if (argc != 1) { std::cerr << "usage: omp_mmdb_core_tests [--fixtures DIRECTORY]\n"; return 2; }
	try {
		CHECK(std::filesystem::is_regular_file(fixtures / "MaxMind-DB-test-decoder.mmdb"));
		test_ip(); test_path_and_utf8(); test_conversion(); test_real_types();
		test_ip_databases(); test_snapshot_and_sustained_queries(); test_bad_databases();
		std::cout << "PASS: " << checks << " checks\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
		return 1;
	}
}
