# Reproducible benchmarks and stress runs

Build the `maxora_benchmark` CMake target in Release mode. It uses the pinned
synthetic MaxMind test fixtures, performs 1,000 warm-up operations per selected
workload, and checks every operation's result. Any unexpected error or resource
leak makes the process exit unsuccessfully.

```sh
./build/maxora_benchmark --iterations 10000 > benchmark.jsonl
./build/maxora_benchmark --seconds 60 > stress.jsonl
./build/maxora_benchmark --seconds 86400 --rate 1000 > soak-24h.jsonl
```

`--seconds` controls the total measured duration, excluding warm-up; all workloads
run interleaved. `--iterations` controls operations per workload and cannot be
combined with `--seconds`. `--rate` limits total operations per second; its
sleep occurs outside latency timing. A paced wall throughput is therefore the
requested soak workload, not the component's maximum capacity. Timing may exceed
the requested duration by one complete round. Override `--fixtures DIRECTORY`
for a different checkout of the same pinned fixtures.

Select one workload with `--workload NAME`:

| Name | One measured operation |
| --- | --- |
| `clock_pair` | Increment a checksum between two clock reads |
| `ipv4_lookup_release` | IPv4 lookup, selected string, release result |
| `ipv6_lookup_release` | IPv6 lookup, selected string, release result |
| `missing_record` | IPv4 lookup expected to find no record |
| `nested_ephemeral_three_fields` | Lookup, three nested field reads, release |
| `nested_reusable_three_fields` | Three reads from a retained lookup result |
| `four_databases` | Rotate IPv4 lookups across four independent mappings |
| `load_reload_unload` | Load, lookup, replace DB, unload, read old snapshot, release |

Output is newline-delimited JSON. Each workload reports operation count, mean
nanoseconds, p50/p95/p99, and operations per second within timed regions. A
deterministic reservoir of up to 65,536 observations per workload bounds memory;
reported percentiles are sample estimates in long runs. The overall wall rate
includes harness overhead and optional pacing. `clock_pair` reports the timing
floor; its overhead is not subtracted. Windows reports working set and peak
working set. Linux reads current RSS from `/proc/self/statm` and peak RSS from
`getrusage`. These process memory snapshots are not allocation counts; allocation
count is explicitly `null` because allocations are not instrumented.

The all-workload run retains nine resources. It periodically verifies this count,
and the lifecycle workload verifies snapshots remain readable after reload and
unload. Generation retirement prevents stale aliases at the expense of a finite
component-lifetime issuance budget: 4,095 slots with 524,287 generations each,
at most 2,146,955,265 resource handles. Reload does not issue a DB handle; each
new lookup result does. A 24-hour soak at 1,000 total operations per second stays
well below this budget. An unlimited-rate prolonged run can intentionally reach
it and should report `ResourceLimit`, not be described as a memory leak.

Record date, OS, CPU, compiler/version, build type, architecture, exact source and
recursive submodule SHAs, commands, concurrent machine activity, and raw JSON.
Use at least five repeated trials on an otherwise idle machine for a reviewed
performance baseline. Synthetic fixtures are small and cache-friendly; these
measurements establish behavior for this workload, not real-world GeoIP capacity.
No v1 speed comparison is claimed. A completed 60-second run is not a completed
24-hour soak or open.mp runtime integration test.
