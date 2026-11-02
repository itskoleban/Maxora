#include "core.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
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
#include <sys/resource.h>
#include <unistd.h>
#endif

#ifndef MAXORA_FIXTURE_DIR
#define MAXORA_FIXTURE_DIR "lib/libmaxminddb/t/maxmind-db/test-data"
#endif

namespace {
using namespace maxora;
using Clock = std::chrono::steady_clock;
std::uint64_t checksum = 0;

void require(Status actual, Status expected = Status::Ok) {
    if (actual != expected) throw std::runtime_error(std::string("unexpected result: ") + status_message(actual));
}
struct Memory { std::uint64_t resident = 0, peak = 0; };
Memory memory() {
    Memory value;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        value.resident = counters.WorkingSetSize;
        value.peak = counters.PeakWorkingSetSize;
    }
#else
    std::ifstream status("/proc/self/statm");
    std::uint64_t total = 0, pages = 0;
    if (status >> total >> pages) {
        const auto page_size = sysconf(_SC_PAGESIZE);
        if (page_size > 0) value.resident = pages * static_cast<std::uint64_t>(page_size);
    }
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0 && usage.ru_maxrss >= 0) {
#if defined(__APPLE__)
        value.peak = static_cast<std::uint64_t>(usage.ru_maxrss);
#else
        value.peak = static_cast<std::uint64_t>(usage.ru_maxrss) * 1024U;
#endif
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
    std::uint64_t percentile(std::size_t percent) const {
        return samples[((samples.size() - 1) * percent) / 100];
    }
    void print() {
        if (count == 0) return;
        std::sort(samples.begin(), samples.end());
        std::cout << "{\"kind\":\"workload\",\"name\":\"" << name
                  << "\",\"operations\":" << count << ",\"mean_ns\":" << static_cast<double>(total_ns / count)
                  << ",\"p50_ns\":" << percentile(50) << ",\"p95_ns\":" << percentile(95)
                  << ",\"p99_ns\":" << percentile(99) << ",\"samples\":" << samples.size()
                  << ",\"timed_throughput_ops_s\":" << static_cast<double>(static_cast<long double>(count) * 1.0e9L / total_ns)
                  << ",\"allocation_count\":null}\n";
    }
};

class Workloads {
public:
    explicit Workloads(const std::filesystem::path& directory)
        : ipv4_path_((directory / "MaxMind-DB-test-ipv4-24.mmdb").string()),
          decoder_path_((directory / "MaxMind-DB-test-decoder.mmdb").string()) {
        require(service_.load(1, ipv4_path_, ipv4_));
        require(service_.load(1, (directory / "MaxMind-DB-test-ipv6-24.mmdb").string(), ipv6_));
        require(service_.load(1, (directory / "MaxMind-DB-test-nested.mmdb").string(), nested_));
        require(service_.load(1, decoder_path_, decoder_));
        require(service_.lookup(1, nested_, "1.1.1.1", reusable_));
        for (auto& handle : multiple_) require(service_.load(1, ipv4_path_, handle));
    }
    static constexpr std::array<const char*, 8> names = {
        "clock_pair", "ipv4_lookup_release", "ipv6_lookup_release", "missing_record",
        "nested_ephemeral_three_fields", "nested_reusable_three_fields", "four_databases", "load_reload_unload"
    };
    void run(std::size_t index) {
        switch (index) {
        case 0: ++checksum; break;
        case 1: query(ipv4_, "1.1.1.3", "/ip"); break;
        case 2: query(ipv6_, "::2:0:1a", "/ip"); break;
        case 3: {
            Handle result = 0;
            require(service_.lookup(1, ipv4_, "2.3.4.5", result), Status::NoRecord);
            ++checksum;
            break;
        }
        case 4: {
            Handle handle = 0;
            require(service_.lookup(1, nested_, "1.1.1.1", handle));
            three_fields(handle);
            require(service_.release(1, handle));
            break;
        }
        case 5: three_fields(reusable_); break;
        case 6: query(multiple_[sequence_++ % multiple_.size()], "1.1.1.3", "/ip"); break;
        case 7: {
            Handle handle = 0;
            require(service_.load(1, ipv4_path_, handle));
            Handle snapshot = 0;
            require(service_.lookup(1, handle, "1.1.1.1", snapshot));
            require(service_.reload(1, handle, decoder_path_));
            require(service_.unload(1, handle));
            // The original mapping must survive reload and DB-handle removal.
            const Lookup* result = nullptr;
            require(service_.result(1, snapshot, result));
            Value value;
            require(result->get("/ip", value));
            checksum += value.data.data_size;
            require(service_.release(1, snapshot));
            break;
        }
        default: throw std::runtime_error("unknown workload");
        }
    }
    void verify() const { if (service_.size() != 9) throw std::runtime_error("resource leak detected"); }
private:
    void query(Handle database, const char* address, const char* path) {
        Handle handle = 0;
        require(service_.lookup(1, database, address, handle));
        const Lookup* found = nullptr;
        require(service_.result(1, handle, found));
        Value value;
        require(found->get(path, value));
        checksum += value.data.data_size + found->prefix;
        require(service_.release(1, handle));
    }
    void three_fields(Handle handle) {
        const Lookup* found = nullptr;
        require(service_.result(1, handle, found));
        for (const char* path : {"/map1/map2/array/0/map3/a", "/map1/map2/array/0/map3/c", "/map1/map2/array/0/map3/a"}) {
            Value value;
            require(found->get(path, value));
            std::int32_t integer = 0;
            require(value.integer(integer));
            checksum += static_cast<std::uint64_t>(integer);
        }
    }
    Service service_;
    std::string ipv4_path_, decoder_path_;
    Handle ipv4_ = 0, ipv6_ = 0, nested_ = 0, decoder_ = 0, reusable_ = 0;
    std::array<Handle, 4> multiple_{};
    std::size_t sequence_ = 0;
};
std::uint64_t parse_integer(const std::string& input) {
    std::size_t consumed = 0;
    if (input.empty() || input[0] == '-') throw std::runtime_error("invalid iteration count");
    const auto value = std::stoull(input, &consumed);
    if (consumed != input.size() || value == 0 || value > 1000000000ULL) throw std::runtime_error("iterations must be 1..1000000000");
    return value;
}
} // namespace

int main(int argc, char** argv) {
    try {
        std::filesystem::path directory = MAXORA_FIXTURE_DIR;
        std::uint64_t iterations = 10000;
        double seconds = 0;
        std::uint64_t rate = 0;
        std::string selected = "all";
        bool explicit_iterations = false;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (i + 1 == argc) throw std::runtime_error("missing option value");
            const std::string value = argv[++i];
            if (option == "--fixtures") directory = value;
            else if (option == "--iterations") { iterations = parse_integer(value); explicit_iterations = true; }
            else if (option == "--seconds") {
                std::size_t used = 0; seconds = std::stod(value, &used);
                if (used != value.size() || !std::isfinite(seconds) || seconds <= 0 || seconds > 172800) throw std::runtime_error("seconds must be >0 and <=172800");
            } else if (option == "--workload") selected = value;
            else if (option == "--rate") rate = parse_integer(value);
            else throw std::runtime_error("unknown option: " + option);
        }
        if (seconds > 0 && explicit_iterations) throw std::runtime_error("choose --iterations or --seconds");
        Workloads workloads(directory);
        std::vector<std::size_t> active;
        std::vector<Statistics> stats;
        for (std::size_t i = 0; i < Workloads::names.size(); ++i) {
            if (selected == "all" || selected == Workloads::names[i]) { active.push_back(i); stats.emplace_back(Workloads::names[i]); }
        }
        if (active.empty()) throw std::runtime_error("unknown workload: " + selected);
        for (std::size_t i = 0; i < 1000; ++i) for (const auto index : active) workloads.run(index);
        workloads.verify();
        const auto before = memory();
        std::cout << std::setprecision(12);
        std::cout << "{\"kind\":\"environment\",\"version\":\"2.0.0\",\"reader\":\"" << MMDB_lib_version()
                  << "\",\"pointer_bits\":" << sizeof(void*) * 8 << ",\"warmup_operations_per_workload\":1000"
                  << ",\"reservoir_capacity\":" << Statistics::capacity
                  << ",\"requested_seconds\":" << seconds << ",\"resident_before_bytes\":" << before.resident
                  << ",\"target_total_operations_per_second\":" << rate
                  << ",\"allocation_note\":\"not instrumented\",\"timer_note\":\"per-operation steady_clock; clock_pair quantifies harness floor\"}\n";
        const auto start = Clock::now();
        const auto deadline = start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
        std::uint64_t rounds = 0;
        do {
            for (std::size_t i = 0; i < active.size(); ++i) {
                const auto begin = Clock::now();
                workloads.run(active[i]);
                const auto end = Clock::now();
                stats[i].add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()));
            }
            ++rounds;
            if (rounds % 1024 == 0) workloads.verify();
            if (rate != 0) {
                const auto paced = std::chrono::duration<double>(static_cast<double>(rounds * active.size()) / static_cast<double>(rate));
                std::this_thread::sleep_until(start + std::chrono::duration_cast<Clock::duration>(paced));
            }
        } while (seconds > 0 ? Clock::now() < deadline : rounds < iterations);
        const auto end = Clock::now();
        workloads.verify();
        const auto after = memory();
        for (auto& statistic : stats) statistic.print();
        const auto elapsed = std::chrono::duration<double>(end - start).count();
        std::cout << "{\"kind\":\"summary\",\"wall_seconds\":" << elapsed
                  << ",\"operations\":" << rounds * active.size()
                  << ",\"wall_throughput_ops_s\":" << static_cast<double>(rounds * active.size()) / elapsed
                  << ",\"resident_after_bytes\":" << after.resident << ",\"peak_resident_bytes\":" << after.peak
                  << ",\"live_resources\":9,\"checksum\":" << checksum << "}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "benchmark failed: " << error.what() << '\n'
                  << "usage: maxora_benchmark [--fixtures DIR] [--iterations N | --seconds N] [--workload NAME] [--rate TOTAL_OPS_PER_SECOND]\n";
        return 1;
    }
}
