# Benchmark and stress runs

Build the `omp_mmdb_benchmark` target in Release mode. The default suite uses
the pinned synthetic MaxMind fixtures and checks the status of every operation.
An unexpected status or resource count fails the run.

```sh
./build/omp_mmdb_benchmark --iterations 100000 > benchmark.jsonl
python3 scripts/check_benchmark.py benchmark.jsonl
./build/omp_mmdb_benchmark --seconds 60 > stress.jsonl
python3 scripts/check_benchmark.py stress.jsonl --minimum-seconds 60
./build/omp_mmdb_benchmark --seconds 86400 --rate 1000 > core-soak.jsonl
```

Use `--fixtures DIRECTORY` to select another checkout of the pinned fixtures.
`--seconds` controls the whole measured run, excluding warm-up. `--iterations`
controls operations per workload. `--rate` limits the total wall rate and
sleeps outside individual latency timings. Select one workload to isolate its
latency and memory measurements.

| `--workload` | One measured operation |
| --- | --- |
| `clock_pair` | Increment a checksum between two clock reads |
| `ipv4_direct` | Find one IPv4 record and read one field |
| `ipv6_direct` | Find one IPv6 record and read one field |
| `missing_record` | Verify that an IPv4 record is absent |
| `nested_direct_three_fields` | Resolve the same address separately for three nested fields |
| `nested_single_lookup_three_fields` | Resolve once and read the same three fields from the temporary C++ lookup |
| `four_databases` | Rotate one-field IPv4 lookups across four named databases |
| `load_reload_unload` | Load, query, replace, unload and read a snapshot held across replacement |
| `concurrent_reload_query` | Read one field while a writer repeatedly opens and publishes replacement snapshots |

The concurrent workload owns a separate registry on its writer thread. It
publishes immutable database references through an atomic shared pointer. The
benchmark reader uses those references without entering the writer's registry.
Only query latency is timed in this workload. The completed reload count
records concurrent activity. This arrangement exercises mapping lifetime
independently of the open.mp thread that owns the production registry.

The report includes mean, p50, p95, p99 and p99.9 in nanoseconds. A deterministic
reservoir retains up to 65,536 samples per workload. The clock-pair latency is
reported without subtracting it from other results. RSS or working set and
process page faults include the harness, reader, mappings, sample buffers and
other process activity. The Windows process counter includes soft and hard
faults together. Linux also reports major faults. Allocation counts are
unmeasured.

## Maxora 2.0 baseline

Build the historical benchmark at the pre-redesign `main` commit with the same
compiler, architecture, build flags and submodule pins. Keep the source and
build directories separate from the candidate.

```sh
git worktree add --detach ../maxora-baseline 661b838
git -C ../maxora-baseline submodule update --init --recursive
cmake -S ../maxora-baseline -B ../build-maxora-baseline -DCMAKE_BUILD_TYPE=Release \
  -DMAXORA_BUILD_COMPONENT=OFF -DMAXORA_BUILD_TESTS=OFF
cmake --build ../build-maxora-baseline --target maxora_benchmark --config Release
cmake -S . -B build-perf -DCMAKE_BUILD_TYPE=Release \
  -DMMDB_BUILD_COMPONENT=OFF -DMMDB_BUILD_TESTS=OFF
cmake --build build-perf --target omp_mmdb_benchmark --config Release
python3 scripts/compare_benchmarks.py \
  --baseline ../build-maxora-baseline/maxora_benchmark \
  --candidate build-perf/omp_mmdb_benchmark \
  --fixtures lib/libmaxminddb/t/maxmind-db/test-data \
  --iterations 10000 --trials 5 --output-dir ../benchmark-comparison
```

Use the actual executable paths for your generator and platform. For example,
Visual Studio Release executables live in `Release/` with an `.exe` extension.
The comparison runs each workload in a separate process, alternates the order
between trials, and checks matching reader version, pointer size, warm-up,
iteration count and checksum. It saves raw JSONL and a median summary. The old
retained-result comparison excludes the initial lookup from its timings. The
candidate's one-lookup workload includes it.

## External databases and first touch

Provide a locally licensed MMDB file and a newline-delimited list of numeric IP
addresses for which the chosen JSON Pointer exists. `external_field` accepts
any provider and reports the file size and separate open time. It aborts if a
selected record or field is missing.

```sh
./build-perf/omp_mmdb_benchmark --workload external_field --database city.mmdb \
  --ips covered-ips.txt --path /country/iso_code --iterations 100000 > large.jsonl
./build-perf/omp_mmdb_benchmark --workload external_field --database city.mmdb \
  --ips covered-ips.txt --path /country/iso_code --warmup 0 --iterations 1 \
  --start-index 0 --drop-cache-hint > first-touch.jsonl
```

The Linux-only `--drop-cache-hint` uses `POSIX_FADV_DONTNEED` on the one input
file before the process opens its mapping. The hint is advisory. A fresh
mapping, major-fault count and query latency are separate observations. A
first-touch result may still come from the system page cache. Repeat with
different `--start-index` values and separate processes. The database open
touches metadata and may touch the search tree before the first query. Use
licensed real data with a broad address sample for working-set measurements.
Sparse padded fixtures can test address-space constraints.

Record the date, OS, CPU, compiler and flags, exact source and submodule SHAs,
database checksums and sizes, address selection, commands and raw JSON. For
current results and historical evidence, see [Performance](../docs/performance.md).
