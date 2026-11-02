#include "core.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef MAXORA_FIXTURE_DIR
#define MAXORA_FIXTURE_DIR "lib/libmaxminddb/t/maxmind-db/test-data"
#endif

namespace {
using namespace maxora;
std::size_t checks = 0;
std::filesystem::path fixtures = MAXORA_FIXTURE_DIR;

void check(bool condition, const char* expression, int line) {
    ++checks;
    if (!condition) throw std::runtime_error(std::string("line ") + std::to_string(line) + ": " + expression);
}
#define CHECK(expression) check((expression), #expression, __LINE__)
#define EXPECT(expression, status) CHECK((expression) == Status::status)

std::string fixture(const char* name) { return (fixtures / name).string(); }
Handle load(Service& service, Owner owner, const char* name) {
    Handle handle = 0;
    EXPECT(service.load(owner, fixture(name), handle), Ok);
    CHECK(handle > 0);
    return handle;
}
Handle lookup(Service& service, Owner owner, Handle db, const char* ip) {
    Handle result = 0;
    EXPECT(service.lookup(owner, db, ip, result), Ok);
    CHECK(result > 0);
    return result;
}
const Lookup& result(Service& service, Owner owner, Handle handle) {
    const Lookup* output = nullptr;
    EXPECT(service.result(owner, handle, output), Ok);
    CHECK(output != nullptr);
    return *output;
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
    Service service;
    const auto db = load(service, 1, "MaxMind-DB-test-decoder.mmdb");
    const auto handle = lookup(service, 1, db, "1.1.1.1");
    const auto& found = result(service, 1, handle);
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
    value = field(found, "/array/-1"); EXPECT(value.integer(integer), Ok); CHECK(integer == 3);
    EXPECT(found.get("/does-not-exist", value), NoField);
    EXPECT(found.get("/array/999", value), NoField);
    EXPECT(found.get("/array/not-a-number", value), NoField);
    EXPECT(found.get("bad", value), InvalidPath);
    value = field(found, ""); CHECK(value.data.type == MMDB_DATA_TYPE_MAP);
    EXPECT(value.string(string), TypeMismatch);
    EXPECT(service.release(1, handle), Ok);
    const auto nested_db = load(service, 1, "MaxMind-DB-test-nested.mmdb");
    const auto nested = lookup(service, 1, nested_db, "1.1.1.1");
    value = field(result(service, 1, nested), "/map1/map2/array/0/map3/c");
    EXPECT(value.integer(integer), Ok); CHECK(integer == 3);
}

void test_ip_databases() {
    Service service;
    for (const char* size : {"24", "28", "32"}) {
        for (const char* family : {"ipv4", "mixed"}) {
            const std::string name = std::string("MaxMind-DB-test-") + family + "-" + size + ".mmdb";
            const auto db = load(service, 1, name.c_str());
            const auto handle = lookup(service, 1, db, "1.1.1.3");
            const auto& found = result(service, 1, handle);
            CHECK(found.prefix == 31);
            std::string_view text;
            EXPECT(field(found, "/ip").string(text), Ok);
            CHECK(text == (std::string_view(family) == "ipv4" ? "1.1.1.2" : "::1.1.1.2"));
            Handle missing = 0;
            EXPECT(service.lookup(1, db, "2.3.4.5", missing), NoRecord);
            if (std::string_view(family) == "ipv4") EXPECT(service.lookup(1, db, "::1", missing), UnsupportedFamily);
            service.clear();
        }
        const std::string name = std::string("MaxMind-DB-test-ipv6-") + size + ".mmdb";
        const auto db = load(service, 1, name.c_str());
        const auto handle = lookup(service, 1, db, "::2:0:1a");
        const auto& found = result(service, 1, handle);
        CHECK(found.prefix == 122);
        std::string_view text;
        EXPECT(field(found, "/ip").string(text), Ok); CHECK(text == "::2:0:0");
        Handle missing = 0;
        EXPECT(service.lookup(1, db, "::abcd", missing), NoRecord);
        service.clear();
    }
}

void test_ownership_and_lifecycle() {
    Service service;
    const auto db = load(service, 11, "MaxMind-DB-test-ipv4-24.mmdb");
    bool valid = false;
    EXPECT(service.valid(11, db, valid), Ok); CHECK(valid);
    EXPECT(service.valid(22, db, valid), WrongOwner);
    EXPECT(service.unload(22, db), WrongOwner);
    EXPECT(service.reload(22, db, fixture("MaxMind-DB-test-ipv4-24.mmdb")), WrongOwner);
    Handle handle = 0;
    EXPECT(service.lookup(22, db, "1.1.1.1", handle), WrongOwner);
    const auto snapshot_handle = lookup(service, 11, db, "1.1.1.1");
    const Lookup* pointer = nullptr;
    EXPECT(service.result(22, snapshot_handle, pointer), WrongOwner);
    EXPECT(service.result(11, db, pointer), InvalidHandle);
    EXPECT(service.unload(11, snapshot_handle), InvalidHandle);
    EXPECT(service.release(11, db), InvalidHandle);
    EXPECT(service.reload(11, db, fixture("does-not-exist.mmdb")), OpenFailed);
    const auto still_valid = lookup(service, 11, db, "1.1.1.1");
    EXPECT(service.release(11, still_valid), Ok);
    EXPECT(service.reload(11, db, fixture("MaxMind-DB-test-decoder.mmdb")), Ok);
    const auto new_result = lookup(service, 11, db, "1.1.1.1");
    CHECK(field(result(service, 11, new_result), "/uint16").data.uint16 == 100);
    std::string_view text;
    EXPECT(field(result(service, 11, snapshot_handle), "/ip").string(text), Ok); CHECK(text == "1.1.1.1");
    EXPECT(service.unload(11, db), Ok);
    EXPECT(field(result(service, 11, snapshot_handle), "/ip").string(text), Ok); CHECK(text == "1.1.1.1");
    EXPECT(service.unload(11, db), InvalidHandle);
    EXPECT(service.release(11, snapshot_handle), Ok);
    EXPECT(service.release(11, snapshot_handle), InvalidHandle);
    const auto fresh = load(service, 11, "MaxMind-DB-test-ipv4-24.mmdb"); CHECK(fresh != db);
    service.clear_owner(11); CHECK(service.size() == 0);
    EXPECT(service.valid(11, fresh, valid), Ok); CHECK(!valid);
    const auto other_owner = load(service, 22, "MaxMind-DB-test-ipv4-24.mmdb");
    const auto another = load(service, 11, "MaxMind-DB-test-ipv4-24.mmdb");
    CHECK(another != fresh);
    service.clear_owner(11); CHECK(service.size() == 1);
    EXPECT(service.valid(22, other_owner, valid), Ok); CHECK(valid);
    service.clear(); CHECK(service.size() == 0);
    const auto after_clear = load(service, 22, "MaxMind-DB-test-ipv4-24.mmdb"); CHECK(after_clear != other_owner);
    for (const Handle invalid : {0, -1, std::numeric_limits<Handle>::min(), std::numeric_limits<Handle>::max()}) {
        EXPECT(service.valid(22, invalid, valid), Ok); CHECK(!valid);
        EXPECT(service.unload(22, invalid), InvalidHandle);
        EXPECT(service.release(22, invalid), InvalidHandle);
    }
    service.clear();
    Handle previous = 0;
    for (int i = 0; i != 2000; ++i) {
        const auto current = load(service, 1, "MaxMind-DB-test-ipv4-24.mmdb");
        CHECK(current != previous);
        if (previous != 0) { EXPECT(service.valid(1, previous, valid), Ok); CHECK(!valid); }
        const auto query = lookup(service, 1, current, "1.1.1.1");
        EXPECT(service.unload(1, current), Ok);
        CHECK(field(result(service, 1, query), "/ip").data.type == MMDB_DATA_TYPE_UTF8_STRING);
        EXPECT(service.release(1, query), Ok);
        CHECK(service.size() == 0);
        previous = current;
    }
}

void test_limits_and_metadata() {
    Service service;
    std::vector<Handle> databases;
    for (std::size_t i = 0; i != Service::max_databases_per_owner; ++i) databases.push_back(load(service, 1, "MaxMind-DB-test-decoder.mmdb"));
    Handle excess = 0;
    EXPECT(service.load(1, fixture("MaxMind-DB-test-decoder.mmdb"), excess), ResourceLimit);
    for (std::size_t i = 0; i != Service::max_lookups_per_owner; ++i) lookup(service, 1, databases[0], "1.1.1.1");
    EXPECT(service.lookup(1, databases[0], "1.1.1.1", excess), ResourceLimit);
    std::string type, epoch;
    std::int32_t version = 0;
    EXPECT(service.metadata(1, databases[0], type, epoch, version), Ok);
    CHECK(!type.empty() && !epoch.empty() && version == 6);
    EXPECT(service.metadata(2, databases[0], type, epoch, version), WrongOwner);
    CHECK(service.size() == Service::max_databases_per_owner + Service::max_lookups_per_owner);
    service.clear_owner(1); CHECK(service.size() == 0);
}

void test_generation_retirement() {
    Service service;
    const auto database = load(service, 1, "MaxMind-DB-test-ipv4-24.mmdb");
    Handle first = 0, latest = 0;
    for (std::uint32_t i = 0; i != 524288U; ++i) {
        Handle handle = 0;
        EXPECT(service.lookup(1, database, "1.1.1.1", handle), Ok);
        if (i == 0) first = handle;
        else CHECK(handle != first);
        latest = handle;
        EXPECT(service.release(1, handle), Ok);
        if (i % 65536U == 0) {
            const Lookup* expired = nullptr;
            EXPECT(service.result(1, first, expired), InvalidHandle);
            CHECK(service.size() == 1);
        }
    }
    CHECK(latest != first);
    const Lookup* expired = nullptr;
    EXPECT(service.result(1, first, expired), InvalidHandle);
    EXPECT(service.result(1, latest, expired), InvalidHandle);
    CHECK(service.size() == 1);
}

void test_global_resource_limit() {
    Service service;
    Status status = Status::Ok;
    Handle final_handle = 0;
    for (Owner owner = 1; owner <= 17 && status == Status::Ok; ++owner) {
        const auto database = load(service, owner, "MaxMind-DB-test-ipv4-24.mmdb");
        for (std::size_t i = 0; i != Service::max_lookups_per_owner; ++i) {
            status = service.lookup(owner, database, "1.1.1.1", final_handle);
            if (status != Status::Ok) break;
        }
    }
    CHECK(status == Status::ResourceLimit);
    CHECK(service.size() == Service::max_resources);
    service.clear(); CHECK(service.size() == 0);
    const auto database = load(service, 1, "MaxMind-DB-test-ipv4-24.mmdb");
    const auto after_clear = lookup(service, 1, database, "1.1.1.1");
    CHECK(after_clear != final_handle);
}

void test_bad_databases() {
    Service service;
    Handle handle = 0;
    EXPECT(service.load(1, "", handle), InvalidArgument);
    EXPECT(service.load(1, std::string_view("file\0.mmdb", 10), handle), InvalidArgument);
    EXPECT(service.load(1, fixture("not-present.mmdb"), handle), OpenFailed);
    const auto temporary = std::filesystem::temp_directory_path() / ("maxora-invalid-" + std::to_string(std::rand()) + ".mmdb");
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code error; std::filesystem::remove(path, error); } } cleanup{temporary};
    for (const std::string& content : {std::string(), std::string("invalid MMDB"), std::string("\xab\xcd\xefMaxMind.com", 14)}) {
        { std::ofstream output(temporary, std::ios::binary | std::ios::trunc); output.write(content.data(), static_cast<std::streamsize>(content.size())); CHECK(output.good()); }
        CHECK(service.load(1, temporary.string(), handle) != Status::Ok);
        CHECK(service.size() == 0);
    }
    // MMDB_open does not validate every record. Traverse each known corrupt
    // location from the pinned upstream fixtures rather than an unaffected IP.
    const auto bad_pointer = load(service, 1, "MaxMind-DB-test-broken-pointers-24.mmdb");
    const auto corrupt = lookup(service, 1, bad_pointer, "1.1.1.16");
    Value value;
    EXPECT(result(service, 1, corrupt).get("", value), DecoderError);
    EXPECT(result(service, 1, corrupt).get("/ip", value), DecoderError);
    EXPECT(service.lookup(1, bad_pointer, "1.1.1.32", handle), DecoderError);
    service.clear();
    const auto bad_tree = load(service, 1, "MaxMind-DB-test-broken-search-tree-24.mmdb");
    EXPECT(service.lookup(1, bad_tree, "255.255.255.255", handle), DecoderError);
    service.clear(); CHECK(service.size() == 0);
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--fixtures") fixtures = argv[2];
    else if (argc != 1) { std::cerr << "usage: maxora_core_tests [--fixtures DIRECTORY]\n"; return 2; }
    try {
        CHECK(std::filesystem::is_regular_file(fixtures / "MaxMind-DB-test-decoder.mmdb"));
        test_ip(); test_path_and_utf8(); test_conversion(); test_real_types();
        test_ip_databases(); test_ownership_and_lifecycle(); test_limits_and_metadata();
        test_generation_retirement(); test_global_resource_limit(); test_bad_databases();
        std::cout << "PASS: " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
