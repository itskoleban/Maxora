#include "pawn_bridge.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

namespace {
using namespace mmdb;

std::size_t checks = 0;
void check(bool value, int line) {
	++checks;
	if (!value) throw std::runtime_error("Native test failed at line " + std::to_string(line));
}
#define CHECK(value) check((value), __LINE__)

constexpr cell name = 0, file = 512, ip = 2048, path = 3072, out = 4108, text = 5000;

const AMX_NATIVE_INFO& native(const char* function) {
	for (int i = 0; i < native_count(); ++i)
		if (std::strcmp(native_table()[i].name, function) == 0) return native_table()[i];
	throw std::runtime_error(std::string("Missing native ") + function);
}

struct Vm {
	std::array<cell, 8192> data{};
	AMX amx{};
	Vm() {
		amx.data = reinterpret_cast<unsigned char*>(data.data());
		amx.hea = 16384;
		amx.stk = 24576;
		amx.stp = 32768;
	}
	cell& at(cell address) { return data[static_cast<std::size_t>(address) / sizeof(cell)]; }
	void string(cell address, std::string_view value) {
		for (std::size_t i = 0; i < value.size(); ++i)
			at(address + static_cast<cell>(i * sizeof(cell))) = static_cast<unsigned char>(value[i]);
		at(address + static_cast<cell>(value.size() * sizeof(cell))) = 0;
	}
	Status call(const char* function, std::initializer_list<cell> args) {
		auto* params = &at(amx.stk);
		params[0] = static_cast<cell>(args.size() * sizeof(cell));
		std::copy(args.begin(), args.end(), params + 1);
		return static_cast<Status>(native(function).func(&amx, params));
	}
};
} // namespace

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
		CHECK(native_count() == 18);

		const auto is_loaded = native("mmdb_is_loaded").func;
		constexpr auto cell_size = static_cast<cell>(sizeof(cell));
		constexpr cell heap_block = 64;
		const auto stack_start = a.amx.stk;
		const std::array<cell, 3> params{2 * cell_size, name, out};
		const auto call_is_loaded = [&](const cell* block) {
			return static_cast<Status>(is_loaded(&a.amx, block));
		};
		const auto call_at = [&](cell address) {
			auto* block = &a.at(address);
			std::copy(params.begin(), params.end(), block);
			return call_is_loaded(block);
		};
		a.string(name, "db");
		a.at(out) = -1;
		CHECK(call_at(heap_block) == Status::InvalidArgument && a.at(out) == -1);
		CHECK(call_at(stack_start + cell_size) == Status::InvalidArgument && a.at(out) == -1);
		CHECK(call_at(stack_start - cell_size) == Status::InvalidArgument && a.at(out) == -1);
		CHECK(call_is_loaded(params.data()) == Status::InvalidArgument);
		CHECK(call_is_loaded(nullptr) == Status::InvalidArgument);

		a.amx.stk = a.amx.stp - 3 * cell_size;
		CHECK(call_at(a.amx.stk) == Status::Ok && a.at(out) == 0);
		a.at(a.amx.stk) = std::numeric_limits<cell>::max();
		CHECK(call_is_loaded(&a.at(a.amx.stk)) == Status::InvalidArgument);
		a.at(a.amx.stk) = params[0];
		a.amx.stp -= cell_size;
		CHECK(call_is_loaded(&a.at(a.amx.stk)) == Status::InvalidArgument);
		a.amx.stp += cell_size;

		a.amx.stk = stack_start + 1;
		std::memcpy(a.amx.data + a.amx.stk, params.data(), params.size() * sizeof(cell));
		const auto* misaligned = reinterpret_cast<const cell*>(a.amx.data + a.amx.stk);
		CHECK(call_is_loaded(misaligned) == Status::InvalidArgument);
		a.amx.stk = heap_block;
		CHECK(call_at(heap_block) == Status::InvalidArgument);
		a.amx.stk = -cell_size;
		CHECK(call_at(heap_block) == Status::InvalidArgument);
		a.amx.stk = stack_start;

		const auto decoder = (std::filesystem::path(MMDB_FIXTURE_DIR) / "MaxMind-DB-test-decoder.mmdb").string();
		const auto ipv4 = (std::filesystem::path(MMDB_FIXTURE_DIR) / "MaxMind-DB-test-ipv4-24.mmdb").string();
		a.string(file, decoder);
		a.string(ip, "1.1.1.1");
		b.string(name, "db");
		b.string(file, ipv4);
		CHECK(a.call("mmdb_load", {name, file}) == Status::Ok);
		CHECK(a.call("mmdb_load", {name, file}) == Status::AlreadyLoaded);
		CHECK(a.call("mmdb_is_loaded", {name, out}) == Status::Ok && a.at(out) == 1);
		CHECK(b.call("mmdb_is_loaded", {name, out}) == Status::Ok && b.at(out) == 0);
		CHECK(b.call("mmdb_unload", {name}) == Status::NotLoaded);
		CHECK(b.call("mmdb_load", {name, file}) == Status::Ok);
		CHECK(b.call("mmdb_get_ip_version", {name, out}) == Status::Ok && b.at(out) == 4);
		CHECK(a.call("mmdb_get_ip_version", {name, out}) == Status::Ok && a.at(out) == 6);

		a.string(path, "/uint16");
		CHECK(a.call("mmdb_has_record", {name, ip, out}) == Status::Ok && a.at(out) == 1);
		CHECK(a.call("mmdb_get_int", {name, ip, path, out}) == Status::Ok && a.at(out) == 100);
		CHECK(a.call("mmdb_get_type", {name, ip, path, out}) == Status::Ok && a.at(out) == MMDB_DATA_TYPE_UINT16);
		CHECK(a.call("mmdb_get_text", {name, ip, path, text, 8, out}) == Status::Ok && a.at(text) == '1');
		a.string(path, "/utf8_string");
		a.at(text) = 777;
		CHECK(a.call("mmdb_get_string", {name, ip, path, text, 1, out}) == Status::BufferTooSmall);
		CHECK(a.at(text) == 777 && a.at(out) > 1);
		CHECK(a.call("mmdb_get_string", {name, ip, path, text, 100, out}) == Status::Ok);
		CHECK(a.call("mmdb_get_size", {name, ip, path, out}) == Status::Ok && a.at(out) > 0);
		CHECK(a.call("mmdb_get_string", {name, ip, path, text, 100, text}) == Status::InvalidArgument);
		CHECK(a.call("mmdb_get_string", {name, ip, path, 16380, 2, out}) == Status::InvalidArgument);
		CHECK(a.call("mmdb_get_string", {name, ip, path, 32764, 2, out}) == Status::InvalidArgument);
		CHECK(a.call("mmdb_get_string", {name, ip, path, text, -1, out}) == Status::InvalidArgument);
		CHECK(a.call("mmdb_get_int", {name, ip, path, 16384}) == Status::InvalidArgument);
		CHECK(a.call("mmdb_get_int", {name, ip, path, out + 1}) == Status::InvalidArgument);
		a.string(path, "/boolean");
		CHECK(a.call("mmdb_get_bool", {name, ip, path, out}) == Status::Ok && a.at(out) == 1);
		a.string(path, "/double");
		CHECK(a.call("mmdb_get_float", {name, ip, path, out}) == Status::Ok);
		a.string(path, "/absent");
		CHECK(a.call("mmdb_has_field", {name, ip, path, out}) == Status::Ok && a.at(out) == 0);
		a.string(path, "malformed");
		CHECK(a.call("mmdb_has_field", {name, ip, path, out}) == Status::InvalidPath);
		CHECK(a.call("mmdb_get_prefix", {name, ip, out}) == Status::Ok);
		CHECK(a.call("mmdb_get_database_type", {name, text, 100, out}) == Status::Ok);
		CHECK(a.call("mmdb_get_build_epoch", {name, text, 100, out}) == Status::Ok);
		CHECK(a.call("mmdb_get_status_message", {0, text, 100, out}) == Status::Ok);
		CHECK(a.call("mmdb_get_status_message", {999, text, 100, out}) == Status::InvalidArgument);
		a.string(ip, "localhost");
		CHECK(a.call("mmdb_has_record", {name, ip, out}) == Status::InvalidIp);

		// Packed strings and unterminated strings cannot cross the AMX heap gap.
		a.at(file) = 0x41424344;
		CHECK(a.call("mmdb_reload", {name, file}) == Status::InvalidArgument);
		a.at(16380) = 'a';
		CHECK(a.call("mmdb_reload", {name, 16380}) == Status::InvalidArgument);
		a.string(name, "bad/name");
		CHECK(a.call("mmdb_is_loaded", {name, out}) == Status::InvalidArgument);
		a.string(name, "db");
		a.string(file, "missing.mmdb");
		CHECK(a.call("mmdb_reload", {name, file}) == Status::OpenFailed);
		CHECK(a.call("mmdb_is_loaded", {name, out}) == Status::Ok && a.at(out) == 1);
		bridge.detach(&a.amx);
		CHECK(service.size() == 1);
		CHECK(a.call("mmdb_unload", {name}) == Status::InvalidArgument);
		CHECK(bridge.attach(&a.amx) == Status::Ok);
		CHECK(a.call("mmdb_is_loaded", {name, out}) == Status::Ok && a.at(out) == 0);
		CHECK(b.call("mmdb_is_loaded", {name, out}) == Status::Ok && b.at(out) == 1);
		CHECK(b.call("mmdb_unload", {name}) == Status::Ok);
		CHECK(service.size() == 0);
		std::cout << checks << " native checks passed\n";
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
