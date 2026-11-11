#include "store.hpp"

#include <atomic>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

using mmdb::Database;
using mmdb::Lookup;
using mmdb::Service;
using mmdb::Status;
using mmdb::Value;

void check(bool condition, const char* expression, int line) {
	if (!condition) throw std::runtime_error("Line " + std::to_string(line) + ": " + expression);
}

#define CHECK(expression) check((expression), #expression, __LINE__)

std::string fixture(const char* name) {
	const auto path = (std::filesystem::path(MMDB_FIXTURE_DIR) / name).u8string();
	return {reinterpret_cast<const char*>(path.data()), path.size()};
}

void test_names_and_transaction() {
	Service service;
	int first = 0, second = 0;
	const auto decoder = fixture("MaxMind-DB-test-decoder.mmdb");
	const auto ipv4 = fixture("MaxMind-DB-test-ipv4-24.mmdb");
	std::shared_ptr<const Database> current;
	CHECK(service.load(&first, "test", decoder) == Status::InvalidArgument);
	CHECK(service.attach(nullptr) == Status::InvalidArgument);
	CHECK(service.attach(&first) == Status::Ok);
	CHECK(service.attach(&second) == Status::Ok);
	for (const auto& invalid : {std::string(), std::string("a/b"), std::string("a b"), std::string(64, 'a')})
		CHECK(service.load(&first, invalid, decoder) == Status::InvalidArgument);

	CHECK(service.load(&first, "test", decoder) == Status::Ok);
	CHECK(service.attach(&first) == Status::Ok);
	CHECK(service.load(&first, "test", decoder) == Status::AlreadyLoaded);
	CHECK(service.database(&first, "Test", current) == Status::NotLoaded);
	CHECK(service.database(&second, "test", current) == Status::NotLoaded);
	CHECK(service.load(&second, "test", ipv4) == Status::Ok);
	CHECK(service.size() == 2);
	CHECK(service.database(&first, "test", current) == Status::Ok);
	CHECK(current->raw().metadata.ip_version == 6);
	const auto original = current;
	CHECK(service.reload(&first, "test", decoder + ".missing") == Status::OpenFailed);
	CHECK(service.database(&first, "test", current) == Status::Ok && current == original);
	CHECK(service.reload(&first, "test", ipv4) == Status::Ok);
	CHECK(service.database(&first, "test", current) == Status::Ok && current != original);
	CHECK(current->raw().metadata.ip_version == 4);
	service.detach(&first);
	CHECK(service.attach(&first) == Status::Ok);
	CHECK(service.database(&first, "test", current) == Status::NotLoaded);
	CHECK(service.database(&second, "test", current) == Status::Ok);
	CHECK(service.unload(&second, "test") == Status::Ok);
	CHECK(service.unload(&second, "test") == Status::NotLoaded);
	CHECK(service.size() == 0);
}

void test_active_limits_and_reuse() {
	Service service;
	int owner = 0;
	const auto decoder = fixture("MaxMind-DB-test-decoder.mmdb");
	CHECK(service.attach(&owner) == Status::Ok);
	for (std::size_t i = 0; i < Service::max_databases_per_script; ++i)
		CHECK(service.load(&owner, std::to_string(i), decoder) == Status::Ok);
	CHECK(service.load(&owner, "extra", decoder) == Status::ResourceLimit);
	CHECK(service.unload(&owner, "0") == Status::Ok);
	CHECK(service.load(&owner, "extra", decoder) == Status::Ok);
	service.clear();
	CHECK(service.size() == 0);

	for (int i = 0; i < 4096; ++i) {
		CHECK(service.attach(&owner) == Status::Ok);
		CHECK(service.load(&owner, "test", decoder) == Status::Ok);
		Lookup lookup;
		CHECK(service.lookup(&owner, "test", "1.1.1.1", lookup) == Status::Ok);
		service.detach(&owner);
		CHECK(service.size() == 0);
	}
}

void test_snapshot_during_reload() {
	Service service;
	int owner = 0;
	const auto decoder = fixture("MaxMind-DB-test-decoder.mmdb");
	const auto ipv4 = fixture("MaxMind-DB-test-ipv4-24.mmdb");
	CHECK(service.attach(&owner) == Status::Ok);
	CHECK(service.load(&owner, "test", decoder) == Status::Ok);
	std::shared_ptr<const Database> original;
	CHECK(service.database(&owner, "test", original) == Status::Ok);
	std::atomic<bool> started{false}, valid{true};

	// The worker only owns an immutable snapshot. Service stays on the caller.
	std::jthread reader([snapshot = std::move(original), &started, &valid](std::stop_token stop) {
		do {
			Lookup lookup;
			Value value;
			std::int32_t number = 0;
			if (Lookup::find(snapshot, "1.1.1.1", lookup) != Status::Ok ||
				lookup.get("/uint16", value) != Status::Ok || value.integer(number) != Status::Ok || number != 100)
				valid = false;
			started = true;
		} while (!stop.stop_requested());
	});
	while (!started) std::this_thread::yield();
	for (int i = 0; i < 128; ++i) {
		CHECK(service.reload(&owner, "test", ipv4) == Status::Ok);
		CHECK(service.reload(&owner, "test", decoder) == Status::Ok);
	}
	service.detach(&owner);
	reader.request_stop();
	reader.join();
	CHECK(valid && service.size() == 0);
}

} // namespace

int main() {
	try {
		test_names_and_transaction();
		test_active_limits_and_reuse();
		test_snapshot_during_reload();
		std::cout << "Database namespace, transaction and snapshot tests passed\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
