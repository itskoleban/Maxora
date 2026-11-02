# Production readiness and validation report

Version 2.0.0; updated November 2, 2026 (America/Bogota). Verdict: **RC validated; stable publication blocked pending continuous 24-hour soak completion and final review.**

## Candidate identity

- Source: c9c5fdd7470f67a93d39afbb3be41c96922eadc3; tag v2.0.0-rc.1.
- [Draft PR 1](https://github.com/itskoleban/Maxora/pull/1); branch rewrite/v2.0.0. Main and v1 releases remain unchanged.
- [Published prerelease](https://github.com/itskoleban/Maxora/releases/tag/v2.0.0-rc.1).
- [Candidate workflow 41529165013](https://github.com/itskoleban/Maxora/actions/runs/41529165013): all nine jobs passed, including tests of downloaded published artifacts on both operating systems.
- [Checksums and artifact identity](validation/candidate-rc1.json).

## Observed gates

| Gate | Actual evidence | Status |
| --- | --- | --- |
| Windows x86 release build | Candidate workflow, compiled module, executed tests | Passed |
| Linux x86 release build | Candidate workflow, compiled module, executed tests | Passed |
| Unit and native-boundary tests | Actual CTest on both OS and x64 development configurations | Passed |
| Actual open.mp integration | Official v1.6.7.3218, compiled 32-bit-cell Pawn, 19 natives, both OS | Passed |
| Lifecycle and resource cleanup | Real filterscript reload/unload/load and GMX; old handles invalidated; snapshots retained | Passed |
| ASan and UBSan | Instrumented core/database CTest and 30-second stress on Linux | Passed |
| Parser/native input fuzzing | Two coverage-guided 60-second libFuzzer runs with raw logs; no reported crashes | Passed |
| Short stress | 60-second mixed benchmark and actual server workloads | Passed |
| Performance evidence review | Pinned fixtures; real timings and memory; [local five-trial review](validation/benchmark-review.md) and [candidate evidence](validation/rc1-benchmark/) | Passed for documented synthetic workload |
| Examples/docs/package verification | Compiled example, 59-member packages, SHA-256/internal manifest and dependency licenses | Passed |
| Published RC artifact tests | Downloaded ZIPs, verified source/member hashes, loaded extracted DLL/SO in actual servers | Passed |
| Bounded correctness/security review | [Findings and fixes](validation/security-review.md); no additional reportable critical/high issue identified | Passed within documented scope |
| Continuous final soak | Actual published Windows DLL; target 86,400 continuous workload seconds | **Running; not passed** |
| Final production review/authorization | Requires completed soak evidence and final reviewed stable gate | **Pending** |

Local Windows x86 final CTest observed 1,604,055 core checks and 91 native checks. The 18 Python release-tool regression tests also pass in the candidate workflow. No proprietary or GeoLite database was used; fixtures are synthetic and redistributable.

The completed pre-candidate [run 41528397618](https://github.com/itskoleban/Maxora/actions/runs/41528397618) is preserved under [ci-5b6c900](validation/ci-5b6c900/). It completed 7,041,343 parser fuzz inputs and 343,015 native fuzz inputs in 61 seconds each. Earlier failed runtime jobs remain failed: test-harness script paths/command delivery/stdout buffering and default 64-bit Pawn compiler cells were corrected before successful validation. Failures were not bypassed.

## Final soak now executing

The independent background runner started November 2 at 21:38:11 UTC, 16:38:11 America/Bogota, using the downloaded published candidate DLL with SHA-256 2954df7df59969fb99a8bde6b16bca3333413fc492ce6fcf366aef75f7d11112. Normal completion is expected after November 3 at about 16:38 America/Bogota if the machine remains available. [Start metadata](validation/soak-start.json) binds package, binary, harness, runtime and fixture hashes. Startup lifecycle checks and fresh sustained workload markers were verified.

Workload: every 100 ms, 30 IPv4 lookup/get/release operations plus missing field, IPv6 lookup/prefix/release and missing IPv4 record; every ten ticks, successful and failed reload, unload/replacement and retained-snapshot checks. Nominal load is 957 native calls/second. Resident server memory is sampled once per second. Final elapsed duration, completed markers, minimum operations, errors, memory and exit status will be recorded by the harness.

Local status: outputs/Maxora-rc1-24h-soak.status.json; final result: outputs/Maxora-rc1-24h-soak.json; workload log: outputs/Maxora-rc1-24h-soak.log. These are local workspace outputs, not completed evidence. A machine shutdown, interrupted process or insufficient duration must fail the gate; separate short runs cannot be stitched into one continuous run.

The first broad automation request was rejected by automatic approval review because it included persistent documentation/GitHub mutations. The user then explicitly authorized a narrower read-only hourly monitor for a maximum of 26 runs. That monitor is active as maxora-rc-soak-read-only-monitor and only observes/notifies on completion, failure or required intervention. It cannot restart the test, modify files/GitHub or publish a release.

## Performance evidence and limits

The actual candidate's core-only x64 hosted benchmark ran on a 4-vCPU AMD EPYC 9V74 Ubuntu runner. Its 60-second mixed run completed 10,768,416 measured operations, with nine retained resources. The 100,000-operation/workload trial measured IPv4 mean 394.249 ns (p50 381, p95 441, p99 581) and IPv6 mean 793.391 ns (p50 772, p95 831, p99 951). These are tiny synthetic-fixture service operations, including a clock-floor workload and mapping-lifecycle work in total throughput; they are not an in-server player capacity claim. Full environment, raw metrics and source pins are in [rc1-benchmark](validation/rc1-benchmark/). CPU allocation, data size, cache warmth and toolchain differ from the local x86 trials, so cross-run speed comparisons are not justified. Allocation counts are unmeasured. Linux current/peak RSS are approximate separate kernel measurements; sanitizer memory includes instrumentation overhead.

Only synchronous main-thread callers are supported. Open does not exhaustively decode or authenticate a database. Mappings must remain immutable; deploy versioned replacement filenames. Packed Pawn strings are rejected; source-level array bounds cannot be discovered at runtime. Integer/float conversion and handle/resource lifetime limits are explicit. See the specification/API/security guides.

## Stable publication gate

[stable.json](validation/stable.json) records the exact candidate commit, passed gates and independently verified package hashes, while leaving soak running and final review unset. Executing release_gate.py correctly blocks on those remaining requirements. Stable publication checks a successful candidate workflow including artifact tests and promotes the same verified package bytes without rebuilding. The final stable tag will be v2.0.0 only after every requirement is demonstrated and authorized. No stable tag, stable release or merge has occurred.

The self-hosted Linux soak workflow remains available for an existing suitable runner; GitHub-hosted jobs have a six-hour limit. The currently executing test uses the available local Windows resource instead of purchasing infrastructure. The [full engineering report](validation/engineering-report.md) covers architecture, API, dependencies, test results, benchmark caveats and delivery references.
