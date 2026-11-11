#include "core.hpp"
#include "store.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#ifndef MMDB_FIXTURE_DIR
#define MMDB_FIXTURE_DIR "lib/libmaxminddb/t/maxmind-db/test-data"
#endif

namespace {
using namespace mmdb;
using Clock = std::chrono::steady_clock;
std::uint64_t checksum = 0;

void require(Status actual, Status expected = Status::Ok) {
	if (actual != expected) throw std::runtime_error(std::string("unexpected result: ") + status_message(actual));
}
struct Memory {
	std::uint64_t resident = 0;
	std::uint64_t peak = 0;
	std::uint64_t page_faults = 0;
	std::optional<std::uint64_t> major_faults;
};
Memory memory() {
	Memory value;
#ifdef _WIN32
	PROCESS_MEMORY_COUNTERS counters{};
	counters.cb = sizeof(counters);
	if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
		value.resident = counters.WorkingSetSize;
		value.peak = counters.PeakWorkingSetSize;
		value.page_faults = counters.PageFaultCount;
	}
#else
	std::ifstream status("/proc/self/statm");
	std::uint64_t total = 0, pages = 0;
	if (status >> total >> pages) {
		const auto page_size = sysconf(_SC_PAGESIZE);
		if (page_size > 0) value.resident = pages * static_cast<std::uint64_t>(page_size);
	}
	struct rusage usage{};
	if (getrusage(RUSAGE_SELF, &usage) == 0) {
		if (usage.ru_minflt >= 0 && usage.ru_majflt >= 0) {
			value.major_faults = static_cast<std::uint64_t>(usage.ru_majflt);
			value.page_faults = static_cast<std::uint64_t>(usage.ru_minflt) + *value.major_faults;
		}
		if (usage.ru_maxrss >= 0) {
#if defined(__APPLE__)
			value.peak = static_cast<std::uint64_t>(usage.ru_maxrss);
#else
			value.peak = static_cast<std::uint64_t>(usage.ru_maxrss) * 1024U;
#endif
		}
	}
#endif
	return value;
}

struct Statistics {
	static constexpr std::size_t capacity = 65536;
	std::string name;
	std::uint64_t count = 0;
	long double total_ns = 0;
	std::uint64_t random = 0x19a72f56d0b3c481ULL;
	std::vector<std::uint64_t> samples;
	explicit Statistics(std::string label) : name(std::move(label)) { samples.reserve(capacity); }
	void add(std::uint64_t duration) {
		++count;
		total_ns += duration;
		if (samples.size() < capacity) samples.push_back(duration);
		else {
			// Deterministic reservoir sampling bounds memory during a 24h run.
			random ^= random << 13U; random ^= random >> 7U; random ^= random << 17U;
			const auto index = random % count;
			if (index < capacity) samples[static_cast<std::size_t>(index)] = duration;
		}
	}
	std::uint64_t percentile(std::size_t per_mille) const {
		return samples[((samples.size() - 1) * per_mille) / 1000];
	}
	void print() {
		if (count == 0) return;
		std::sort(samples.begin(), samples.end());
		std::cout << "{\"kind\":\"workload\",\"name\":\"" << name
				  << "\",\"operations\":" << count << ",\"mean_ns\":" << static_cast<double>(total_ns / count)
				  << ",\"p50_ns\":" << percentile(500) << ",\"p95_ns\":" << percentile(950)
				  << ",\"p99_ns\":" << percentile(990) << ",\"p999_ns\":" << percentile(999) << ",\"samples\":" << samples.size()
				  << ",\"timed_throughput_ops_s\":" << (total_ns > 0 ? static_cast<double>(static_cast<long double>(count) * 1.0e9L / total_ns) : 0)
				  << ",\"allocation_count\":null}\n";
	}
};

std::string utf8_path(const std::filesystem::path& path) {
	const auto text = path.u8string();
	return {text.begin(), text.end()};
}

void advise_drop_cache(const std::filesystem::path& path) {
#if defined(__linux__)
	const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0) throw std::runtime_error("cannot open input for cache advice");
	const int result = posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
	close(fd);
	if (result != 0) throw std::runtime_error("posix_fadvise failed: " + std::to_string(result));
#else
	(void)path;
	throw std::runtime_error("--drop-cache-hint requires Linux");
#endif
}

class ExternalWorkload {
public:
	ExternalWorkload(const std::filesystem::path& file, const std::filesystem::path& ips, std::string path,
		bool drop_cache_hint, std::size_t start_index)
		: path_(std::move(path)), size_(std::filesystem::file_size(file)) {
		std::ifstream addresses(ips);
		std::string line;
		while (std::getline(addresses, line)) {
			if (!line.empty() && line.back() == '\r') line.pop_back();
			IpAddress parsed;
			require(IpAddress::parse(line, parsed));
			ips_.push_back(std::move(line));
		}
		if (!addresses.eof() || start_index >= ips_.size()) throw std::runtime_error("invalid or empty IP list");
		index_ = start_index;
		if (drop_cache_hint) advise_drop_cache(file);
		require(service_.attach(this));
		const auto begin = Clock::now();
		require(service_.load(this, "external", utf8_path(file)));
		open_ns_ = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - begin).count();
	}

	static constexpr std::array<const char*, 1> names{"external_field"};

	void prepare(std::size_t) const noexcept {}
	void finish() const noexcept {}
	void verify() const {
		if (size() != 1) throw std::runtime_error("resource leak detected");
	}

	void run(std::size_t) {
		Lookup record;
		require(service_.lookup(this, "external", ips_[index_], record));
		Value value;
		require(record.get(path_, value));
		checksum += value.data.data_size + record.prefix;
		index_ = (index_ + 1) % ips_.size();
	}
	std::uintmax_t file_size() const noexcept { return size_; }
	std::int64_t open_ns() const noexcept { return open_ns_; }
	std::size_t count() const noexcept { return ips_.size(); }
	std::size_t size() const noexcept { return service_.size(); }
	static std::uint64_t reloads() noexcept { return 0; }

private:
	Service service_;
	std::string path_;
	std::uintmax_t size_;
	std::int64_t open_ns_ = 0;
	std::vector<std::string> ips_;
	std::size_t index_ = 0;
};

class ConcurrentReloads {
public:
	ConcurrentReloads(const std::string& primary, const std::string& replacement) {
		std::promise<void> ready;
		auto startup = ready.get_future();
		worker_ = std::jthread([this, primary, replacement, ready = std::move(ready)](std::stop_token stop) mutable {
			// The registry has one owner thread. The reader only holds immutable mappings.
			Service service;
			try {
				require(service.attach(this));
				require(service.load(this, "city", primary));
				publish(service);
				ready.set_value();
			} catch (...) {
				ready.set_exception(std::current_exception());
				return;
			}
			try {
				const std::array paths{replacement, primary};
				std::size_t index = 0;
				while (!stop.stop_requested()) {
					require(service.reload(this, "city", paths[index]));
					publish(service);
					completed_.fetch_add(1, std::memory_order_relaxed);
					index = 1 - index;
				}
			} catch (...) {
				failed_.store(true, std::memory_order_relaxed);
			}
		});
		startup.get();
	}

	void query() const {
		Lookup record;
		require(Lookup::find(current_.load(), "1.1.1.3", record));
		Value value;
		require(record.get("/ip", value));
		checksum += value.data.data_size + record.prefix;
	}

	void verify() const {
		if (failed_.load(std::memory_order_relaxed)) throw std::runtime_error("concurrent reload failed");
	}

	void stop() {
		worker_.request_stop();
		worker_.join();
	}

	std::uint64_t count() const noexcept { return completed_.load(std::memory_order_relaxed); }

private:
	void publish(const Service& service) {
		std::shared_ptr<const Database> database;
		require(service.database(this, "city", database));
		current_.store(std::move(database));
	}

	std::atomic<std::shared_ptr<const Database>> current_;
	std::atomic<std::uint64_t> completed_{0};
	std::atomic<bool> failed_{false};
	std::jthread worker_;
};

class Workloads {
public:
	explicit Workloads(const std::filesystem::path& directory)
		: ipv4_path_(utf8_path(directory / "MaxMind-DB-test-ipv4-24.mmdb")),
		  replacement_path_(utf8_path(directory / "MaxMind-DB-test-ipv4-28.mmdb")),
		  decoder_path_(utf8_path(directory / "MaxMind-DB-test-decoder.mmdb")) {
		require(service_.attach(this));
		require(service_.load(this, "ipv4", ipv4_path_));
		require(service_.load(this, "ipv6", utf8_path(directory / "MaxMind-DB-test-ipv6-24.mmdb")));
		require(service_.load(this, "nested", utf8_path(directory / "MaxMind-DB-test-nested.mmdb")));
		require(service_.load(this, "decoder", decoder_path_));
		for (const auto name : multiple_) require(service_.load(this, name, ipv4_path_));
	}

	static constexpr std::array<const char*, 9> names = {
		"clock_pair", "ipv4_direct", "ipv6_direct", "missing_record", "nested_direct_three_fields",
		"nested_single_lookup_three_fields", "four_databases", "load_reload_unload", "concurrent_reload_query"
	};
	static constexpr std::size_t concurrent_index = names.size() - 1;

	void prepare(std::size_t index) {
		if (index == concurrent_index && !concurrent_) {
			concurrent_ = std::make_unique<ConcurrentReloads>(ipv4_path_, replacement_path_);
		}
	}

	void run(std::size_t index) {
		switch (index) {
		case 0: ++checksum; break;
		case 1: query("ipv4", "1.1.1.3"); break;
		case 2: query("ipv6", "::2:0:1a"); break;
		case 3: {
			Lookup record;
			require(service_.lookup(this, "ipv4", "2.3.4.5", record), Status::NoRecord);
			++checksum;
			break;
		}
		case 4:
			for (const auto path : nested_paths_) {
				Lookup record;
				require(service_.lookup(this, "nested", "1.1.1.1", record));
				read_nested(record, path);
			}
			break;
		case 5: {
			Lookup record;
			require(service_.lookup(this, "nested", "1.1.1.1", record));
			for (const auto path : nested_paths_) read_nested(record, path);
			break;
		}
		case 6: query(multiple_[sequence_++ % multiple_.size()], "1.1.1.3"); break;
		case 7: {
			require(service_.load(this, "temporary", ipv4_path_));
			Lookup snapshot;
			require(service_.lookup(this, "temporary", "1.1.1.1", snapshot));
			require(service_.reload(this, "temporary", decoder_path_));
			require(service_.unload(this, "temporary"));
			// An active C++ lookup keeps the old mapping alive across a replacement.
			Value value;
			require(snapshot.get("/ip", value));
			checksum += value.data.data_size;
			break;
		}
		case concurrent_index: concurrent_->query(); break;
		default: throw std::runtime_error("unknown workload");
		}
	}

	void verify() const {
		if (size() != 8) throw std::runtime_error("resource leak detected");
		if (concurrent_) concurrent_->verify();
	}

	void finish() {
		if (concurrent_) concurrent_->stop();
	}

	std::size_t size() const noexcept { return service_.size(); }
	std::uint64_t reloads() const noexcept { return concurrent_ ? concurrent_->count() : 0; }

private:
	void query(std::string_view database, std::string_view address) const {
		Lookup record;
		require(service_.lookup(this, database, address, record));
		Value value;
		require(record.get("/ip", value));
		checksum += value.data.data_size + record.prefix;
	}

	static void read_nested(const Lookup& record, std::string_view path) {
		Value value;
		require(record.get(path, value));
		std::int32_t integer = 0;
		require(value.integer(integer));
		checksum += static_cast<std::uint64_t>(integer);
	}

	static constexpr std::array<std::string_view, 4> multiple_{"one", "two", "three", "four"};
	static constexpr std::array<std::string_view, 3> nested_paths_{
		"/map1/map2/array/0/map3/a", "/map1/map2/array/0/map3/c", "/map1/map2/array/0/map3/a"
	};
	Service service_;
	std::string ipv4_path_, replacement_path_, decoder_path_;
	std::size_t sequence_ = 0;
	std::unique_ptr<ConcurrentReloads> concurrent_;
};

struct Options {
	std::filesystem::path fixtures = MMDB_FIXTURE_DIR;
	std::filesystem::path database;
	std::filesystem::path ips;
	std::optional<std::string> field;
	std::string workload = "all";
	std::uint64_t iterations = 10000;
	std::uint64_t rate = 0;
	std::size_t warmup = 1000;
	std::size_t start_index = 0;
	double seconds = 0;
	bool drop_cache_hint = false;
};

std::uint64_t parse_count(const std::string& input, std::uint64_t minimum) {
	std::size_t consumed = 0;
	if (input.empty() || input.find_first_not_of("0123456789") != std::string::npos) {
		throw std::runtime_error("invalid count");
	}
	const auto value = std::stoull(input, &consumed);
	if (consumed != input.size() || value < minimum || value > 1000000000ULL) {
		throw std::runtime_error("count outside supported range");
	}
	return value;
}

Options parse_options(int argc, char** argv) {
	Options options;
	bool explicit_iterations = false;
	for (int i = 1; i < argc; ++i) {
		const std::string option = argv[i];
		if (option == "--drop-cache-hint") {
			options.drop_cache_hint = true;
			continue;
		}
		if (i + 1 == argc) throw std::runtime_error("missing option value");
		const std::string value = argv[++i];
		if (option == "--fixtures") options.fixtures = value;
		else if (option == "--database") options.database = value;
		else if (option == "--ips") options.ips = value;
		else if (option == "--path") options.field = value;
		else if (option == "--warmup") options.warmup = static_cast<std::size_t>(parse_count(value, 0));
		else if (option == "--start-index") options.start_index = static_cast<std::size_t>(parse_count(value, 0));
		else if (option == "--iterations") {
			options.iterations = parse_count(value, 1);
			explicit_iterations = true;
		} else if (option == "--seconds") {
			std::size_t used = 0;
			options.seconds = std::stod(value, &used);
			if (used != value.size() || !std::isfinite(options.seconds) || options.seconds <= 0 || options.seconds > 172800) {
				throw std::runtime_error("seconds must be >0 and <=172800");
			}
		} else if (option == "--workload") options.workload = value;
		else if (option == "--rate") options.rate = parse_count(value, 1);
		else throw std::runtime_error("unknown option: " + option);
	}
	if (options.seconds > 0 && explicit_iterations) throw std::runtime_error("choose --iterations or --seconds");
	const bool external = options.workload == "external_field";
	if (external && (options.database.empty() || options.ips.empty() || !options.field)) {
		throw std::runtime_error("external_field requires --database, --ips and --path");
	}
	if (!external && (!options.database.empty() || !options.ips.empty() || options.field || options.drop_cache_hint || options.start_index != 0)) {
		throw std::runtime_error("external database options require --workload external_field");
	}
	return options;
}

template<class WorkloadSet>
void measure(const Options& options, WorkloadSet& workloads) {
	std::vector<std::size_t> active;
	std::vector<Statistics> stats;
	for (std::size_t i = 0; i < WorkloadSet::names.size(); ++i) {
		if (options.workload == "all" || options.workload == WorkloadSet::names[i]) {
			active.push_back(i);
			stats.emplace_back(WorkloadSet::names[i]);
		}
	}
	if (active.empty()) throw std::runtime_error("unknown workload: " + options.workload);
	for (std::size_t i = 0; i < options.warmup; ++i) {
		for (const auto index : active) {
			workloads.prepare(index);
			workloads.run(index);
		}
	}
	workloads.verify();
	const auto before = memory();
	const auto reloads_before = workloads.reloads();
	std::cout << std::setprecision(12)
		<< "{\"kind\":\"environment\",\"project\":\"omp-mmdb\",\"reader\":\"" << MMDB_lib_version()
		<< "\",\"pointer_bits\":" << sizeof(void*) * 8
		<< ",\"warmup_operations_per_workload\":" << options.warmup
		<< ",\"reservoir_capacity\":" << Statistics::capacity
		<< ",\"requested_seconds\":" << options.seconds << ",\"resident_before_bytes\":" << before.resident
		<< ",\"target_total_operations_per_second\":" << options.rate
		<< ",\"allocation_note\":\"not instrumented\",\"timer_note\":\"per-operation steady_clock; clock_pair measures harness floor\"}\n";
	const auto start = Clock::now();
	const auto deadline = start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(options.seconds));
	std::uint64_t rounds = 0;
	do {
		for (std::size_t i = 0; i < active.size(); ++i) {
			workloads.prepare(active[i]);
			const auto begin = Clock::now();
			workloads.run(active[i]);
			const auto end = Clock::now();
			stats[i].add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()));
		}
		++rounds;
		if (rounds % 1024 == 0) workloads.verify();
		if (options.rate != 0) {
			const auto paced = std::chrono::duration<double>(static_cast<double>(rounds * active.size()) / static_cast<double>(options.rate));
			std::this_thread::sleep_until(start + std::chrono::duration_cast<Clock::duration>(paced));
		}
	} while (options.seconds > 0 ? Clock::now() < deadline : rounds < options.iterations);
	const auto end = Clock::now();
	workloads.finish();
	workloads.verify();
	const auto after = memory();
	for (auto& statistic : stats) statistic.print();
	const auto elapsed = std::chrono::duration<double>(end - start).count();
	const auto faults = after.page_faults >= before.page_faults ? after.page_faults - before.page_faults : 0;
	const auto major_faults = after.major_faults && before.major_faults && *after.major_faults >= *before.major_faults
		? std::to_string(*after.major_faults - *before.major_faults) : "null";
	std::cout << "{\"kind\":\"summary\",\"wall_seconds\":" << elapsed
		<< ",\"operations\":" << rounds * active.size()
		<< ",\"wall_throughput_ops_s\":" << (elapsed > 0 ? static_cast<double>(rounds * active.size()) / elapsed : 0)
		<< ",\"resident_after_bytes\":" << after.resident << ",\"peak_resident_bytes\":" << after.peak
		<< ",\"process_page_faults\":" << faults << ",\"process_major_page_faults\":" << major_faults
		<< ",\"live_resources\":" << workloads.size() << ",\"checksum\":" << checksum
		<< ",\"concurrent_reloads_completed\":" << workloads.reloads() - reloads_before << "}\n";
}
} // namespace

int main(int argc, char** argv) {
	try {
		const auto options = parse_options(argc, argv);
		if (options.workload == "external_field") {
			ExternalWorkload workload(options.database, options.ips, *options.field, options.drop_cache_hint, options.start_index);
			std::cout << "{\"kind\":\"external_database\",\"file_size_bytes\":" << workload.file_size()
				<< ",\"open_ns\":" << workload.open_ns() << ",\"ip_count\":" << workload.count()
				<< ",\"start_index\":" << options.start_index << ",\"drop_cache_hint\":" << std::boolalpha << options.drop_cache_hint << "}\n";
			measure(options, workload);
		} else {
			Workloads workloads(options.fixtures);
			measure(options, workloads);
		}
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "benchmark failed: " << error.what() << '\n'
			<< "usage: omp_mmdb_benchmark [--fixtures DIR] [--iterations N | --seconds N] "
			   "[--workload NAME] [--rate TOTAL_OPS_PER_SECOND] [--warmup N]\n"
			<< "external_field also takes --database FILE --ips FILE --path POINTER [--start-index N] [--drop-cache-hint]\n";
		return 1;
	}
}
