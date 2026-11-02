# Performance and benchmark methodology

The benchmark executable is a synchronous C++ service harness using the pinned synthetic MaxMind fixtures. It measures parser/library/resource-table operations rather than network traffic or geographic accuracy. It does not establish capacity for every open.mp server, database edition or storage device.

```sh
cmake -S . -B build-perf -G Ninja -DMAXORA_BUILD_COMPONENT=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-perf
build-perf/maxora_benchmark --iterations 100000 > benchmark.jsonl
python3 scripts/check_benchmark.py benchmark.jsonl
build-perf/maxora_benchmark --seconds 60 > stress.jsonl
```

The eight workloads are clock-pair measurement, IPv4 lookup/release, IPv6 lookup/release, missing record, nested lookup with three fields and a temporary result, three fields using a reusable result, four loaded databases, and load/reload/unload with snapshot lifetime checks. Each workload has 1,000 warmup operations. The program records total operations, mean and p50/p95/p99 nanoseconds, timed throughput, overall wall throughput and process resident/peak memory where available. Percentiles use a bounded reservoir; record its emitted capacity and sample count. Timing includes the harness clock overhead and should be compared with its `clock_pair` workload. Allocation behavior is explicitly not instrumented; no zero-allocation claim is made.

Archive JSON lines together with source commit, recursive submodule commits, compiler/version/flags, OS build, CPU, memory, storage, process affinity, database hashes and whether the file cache was warm. Repeat on the same controlled machine to compare changes. Hosted-runner timing is collected for inspection, with structural/resource invariants enforced automatically; it is too variable for an honest narrow percentage regression gate. Review sustained or large timing changes using matched environments before release. The pipeline does not claim a speedup over 1.x: the APIs and fixture-driven workloads differ and no fair legacy baseline has been established.

For a representative long service test, use:

```sh
build-perf/maxora_benchmark --seconds 86400 --rate 1000 > core-soak.jsonl
python3 scripts/check_benchmark.py core-soak.jsonl --minimum-seconds 86400
```

`--seconds` applies to the whole mixed workload; `--rate` is target total operations per second, not per workload. The paced run represents a documented load and avoids exhausting the deliberately finite handle issuance budget during an unbounded synthetic churn loop. Handles never wrap: slots retire when generation space is exhausted, so operators must restart after an extreme lifetime of successful resource issuance. The precise limit and quotas are documented in the architecture/API guide.

A core soak supplements the required 24-hour in-server release-candidate soak. Run the latter through `scripts/openmp_integration.py` using the exact extracted candidate component. Document its duration, timer workload, reload activity, counters, process memory and errors. Do not mark a 24-hour gate passed from a short stress test or idle server uptime. Actual measurements belong in [production-readiness.md](production-readiness.md) and linked raw evidence files.
