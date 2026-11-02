# Benchmark evidence review

Review date: 2026-11-02, America/Bogota. Verdict: **local smoke measurements completed; these are not a release-candidate performance qualification**.

## Scope and provenance

The C++ service benchmark exercises the pinned synthetic MMDB fixtures directly. It does not include open.mp dispatch, Pawn string marshalling, players, network traffic, production GeoIP database size, or geographic accuracy. The fixture workload is small and cache-friendly.

All ten completed JSON-lines files in `benchmarks/results/` passed `scripts/check_benchmark.py`: eight distinct workloads, positive operation/sample measurements, ordered percentiles, matching summary operation totals and the expected nine retained resources. This validates the recorded structure and invariant checks; it does not turn the figures into a controlled baseline.

The five Windows x86 trials and paced run use the environment recorded in [windows-x86-environment.txt](../../benchmarks/results/windows-x86-environment.txt):

- OS: Microsoft Windows NT 10.0.26300.0.
- CPU: Intel64 Family 6 Model 186 Stepping 2, GenuineIntel; 12 logical processors. A more specific CPU brand, physical memory, storage model, affinity and power policy were not captured.
- Build: Release, MSVC 19.51.36248.0, Win32, Windows SDK 10.0.26100.0, warnings as errors and `/guard:cf`.
- Reader: libmaxminddb 1.14.1, commit `4a3d17a621c0ebcb81498fd34d313f67fd940ab5`.
- Fixtures: MaxMind-DB commit `dc32db5525f9c42ab7006ab3f13c69330b814dd1`.
- Recorded source base: `fd1f134703f2915a9297e88469e2d1cea5f74edd`, with concurrent working-tree development. The environment file records executable and source hashes, but these were not captured by an immutable build-provenance step. Do not attribute these measurements to final candidate bytes solely from those snapshots.
- Commands: five separate `maxora_benchmark --iterations 1000` runs; one `maxora_benchmark --seconds 60 --rate 1000` run, outside the restricted execution sandbox.

Concurrent engineering activity was present. The machine was not isolated or otherwise idle. Trial sample sizes are modest and their measured phases last less than one second. The results show harness behavior and justify further controlled measurement; they do not establish a maximum server capacity or an acceptable regression threshold.

## Five Windows x86 trials

Every trial warms each workload 1,000 times, then measures 1,000 operations per workload: 8,000 measured operations in total. Each reported percentile therefore uses 1,000 observations in these trials, below the 65,536-observation reservoir capacity. Clock overhead is included and is not subtracted.

The table reports the **median of five per-trial statistics**, followed by their minimum–maximum range in brackets. Latency units are microseconds. A median of per-trial percentiles is not a pooled percentile across all observations. Timed throughput is the median per-trial operation rate inside the timed regions, excluding interleaved workloads and outer-loop overhead.

| Workload | Mean µs | p50 µs | p95 µs | p99 µs | Timed ops/s |
| --- | ---: | ---: | ---: | ---: | ---: |
| Clock pair | 0.192 [0.138–0.220] | 0.200 [0.100–0.200] | 0.400 [0.300–0.500] | 0.500 [0.400–0.700] | 5,202,914 [4,541,326–7,267,442] |
| IPv4 lookup, field, release | 1.976 [1.787–2.336] | 1.800 [1.600–2.000] | 4.100 [3.400–5.400] | 5.200 [4.700–7.200] | 506,124 [428,027–559,660] |
| IPv6 lookup, field, release | 3.279 [2.649–3.674] | 3.000 [2.600–3.200] | 5.400 [5.000–7.100] | 7.200 [5.800–9.300] | 304,999 [272,153–377,487] |
| Missing record | 0.418 [0.343–0.498] | 0.400 [0.300–0.400] | 0.800 [0.700–1.100] | 1.000 [0.900–1.400] | 2,390,629 [2,008,435–2,917,153] |
| Nested, temporary result, three fields | 3.383 [2.872–4.306] | 3.200 [2.800–3.500] | 6.100 [5.500–8.100] | 8.100 [6.500–11.700] | 295,639 [232,250–348,141] |
| Nested, retained result, three fields | 1.795 [1.519–1.981] | 1.800 [1.300–1.900] | 2.500 [2.100–3.700] | 3.500 [2.500–5.100] | 557,196 [504,898–658,285] |
| Rotate across four databases | 1.077 [0.959–1.284] | 1.100 [0.900–1.200] | 1.600 [1.500–2.400] | 2.200 [1.900–3.100] | 928,505 [778,816–1,043,079] |
| Load, reload, unload and snapshot read | 598.444 [498.600–689.697] | 595.300 [502.400–630.900] | 1,041.300 [898.200–1,390.900] | 1,316.400 [1,125.800–1,906.100] | 1,671 [1,450–2,006] |

The retained-result workload avoids a new lookup/result lifecycle and had lower measured latency than its temporary-result counterpart in all five local trials. That is evidence for the reusable-result API's intended use in this fixture workload. It is not a claim that every application, database, or field sequence has the same improvement.

Whole-run wall throughput has median **13,076.9 operations/s**, range **11,339.7–15,685.9 operations/s**, over measured durations **0.5100111–0.7054859 seconds**. The expensive mapping lifecycle operation is interleaved once per round and dominates this mixed rate. The clock-pair workload also contributes to the total operation count, so this number must not be described as an IP lookup rate.

Raw trials: [1](../../benchmarks/results/windows-x86-trial-1.jsonl), [2](../../benchmarks/results/windows-x86-trial-2.jsonl), [3](../../benchmarks/results/windows-x86-trial-3.jsonl), [4](../../benchmarks/results/windows-x86-trial-4.jsonl), [5](../../benchmarks/results/windows-x86-trial-5.jsonl).

## Memory and the paced Windows x86 run

Windows measurements are process working-set snapshots, including the benchmark, reader, mappings and statistics buffers. They are not the component's incremental memory cost, allocation counts, or proof that every allocation was freed.

| Run | Before bytes | After bytes | Peak bytes | Measured operations | Retained resources |
| --- | ---: | ---: | ---: | ---: | ---: |
| Trial 1 | 6,361,088 | 6,467,584 | 6,475,776 | 8,000 | 9 |
| Trial 2 | 5,890,048 | 5,992,448 | 6,000,640 | 8,000 | 9 |
| Trial 3 | 5,894,144 | 5,996,544 | 6,004,736 | 8,000 | 9 |
| Trial 4 | 5,898,240 | 6,000,640 | 6,008,832 | 8,000 | 9 |
| Trial 5 | 5,898,240 | 6,000,640 | 6,008,832 | 8,000 | 9 |
| Paced 60-second run | 5,910,528 | 6,373,376 | 6,381,568 | 60,000 | 9 |

The [paced run](../../benchmarks/results/windows-x86-paced-stress-60s.jsonl) completed in **60.001508 seconds**, with **999.9749 total operations/s** against a target of 1,000. Each of the eight workloads completed 7,500 measured operations. Resource-count and snapshot-read checks remained successful. Before/after working set increased by 462,848 bytes; the harness grows its bounded sample storage during measurement, and these snapshots alone do not identify the cause of every byte change. Allocation counts are explicitly `null` and were not instrumented.

This verifies only a short paced service workload. It is not a 24-hour soak, a saturation test, a player simulation, an in-server native benchmark, or final candidate artifact validation.

## Exploratory Windows x64 runs

The [x64 environment](../../benchmarks/results/windows-x64-environment.txt) uses GCC 16.1.0 from MSYS2, Release, with the component disabled. Windows x64 is a core development configuration, not a distributed component target. Source-base/working-tree provenance has the same limitations described above.

| Raw evidence | Measured operations | Wall seconds | Total wall ops/s | Before → after working set, bytes | Peak bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| [Initial 100,000/workload](../../benchmarks/results/windows-x64-initial.jsonl) | 800,000 | 358.8441781 | 2,229.3799 | 6,291,456 → 10,604,544 | 10,612,736 |
| [Initial 1,000/workload](../../benchmarks/results/windows-x64-initial-1000.jsonl) | 8,000 | 0.5026881 | 15,914.4408 | 6,021,120 → 6,176,768 | 6,184,960 |
| [Unpaced 60 seconds](../../benchmarks/results/windows-x64-stress-60s.jsonl) | 194,016 | 60.0025395 | 3,233.4631 | 6,029,312 → 7,630,848 | 7,639,040 |
| [Paced 60 seconds](../../benchmarks/results/windows-x64-paced-stress-60s.jsonl) | 60,000 | 60.0063498 | 999.8942 | 6,299,648 → 6,823,936 | 6,868,992 |

The initial long run and shorter exploratory runs used different execution restrictions/concurrent host conditions. Toolchain, bitness, timing conditions and mapping overhead also differ from the x86 trials. Do not compare these figures as an x86-versus-x64 result, or interpret their differences as a Maxora code regression. Every completed run reports nine live resources after its internal checks.

## Remaining evidence

For release qualification, run at least five longer trials on a controlled machine against the exact candidate build, preserve its full source/build/fixture provenance, and review its workload-specific tail behavior. Record physical memory, storage, affinity, power policy and file-cache conditions. Include realistic provider databases only where their licenses permit the experiment and redistribution of its evidence.

Linux CI performance and sanitizer/stress artifacts are separate evidence and must be reviewed against their own commit and environment. The required continuous 24-hour test must exercise the actual downloaded candidate component inside open.mp, with documented workload, counters, memory and errors. None of the local runs in this review satisfies that final gate. There is no fair Maxora 1.x baseline and no speedup, zero-allocation, lock-free, thread-safe or unlimited-scalability claim.
