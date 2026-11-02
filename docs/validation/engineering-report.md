# Maxora 2.0.0 engineering delivery report

Date: 2026-11-02 (America/Bogota).

**Verdict: implementation and initial cross-platform validation completed; stable publication remains blocked by the final continuous 24-hour candidate soak and its final evidence review.** No stable v2.0.0 release has been published or merged into main.

## Delivery identity

- Repository: https://github.com/itskoleban/Maxora
- Development branch: rewrite/v2.0.0
- Draft review PR: https://github.com/itskoleban/Maxora/pull/1
- Candidate tag: v2.0.0-rc.1
- Candidate source commit: c9c5fdd7470f67a93d39afbb3be41c96922eadc3
- Candidate pipeline: https://github.com/itskoleban/Maxora/actions/runs/41529165013
- Completed pre-candidate validation: https://github.com/itskoleban/Maxora/actions/runs/41528397618

The original MIT license, copyright, Git history, main branch and v1.0.0/v1.1.0/v1.1.1 tags/releases were preserved. Six focused commits implement the rewrite and validation infrastructure. Nothing has been merged into the production branch.

## Final architecture

This is a new implementation, not an adaptation of the previous static store. The open.mp component owns a service and native bridge. The component handles SDK dependency resolution, registration, script events and shutdown. The bridge validates AMX parameter blocks, allocated memory spans and values, routes each script to a monotonic owner identity, catches exceptions and copies checked outputs. The core has no SDK dependency and owns RAII database mappings, bounded IP/path parsers, conversions and a typed resource table.

Database resources and reusable lookup resources share a handle namespace. A positive 32-bit handle contains a 12-bit slot and 19-bit generation. Invalidation advances the generation; slots retire before wrap. Resource kind and script ownership are checked on every access. Clearing resources never resets generations. Each script can own 64 database handles and 256 results;4,095 slots are available globally. The finite lifetime issuance budget is 4,095Ã—524,287 successful creations, after which ResourceLimit is returned rather than aliasing stale handles.

A lookup pins an immutable database mapping. Successful reload opens and validates a replacement before swapping the database resource; failure preserves the original. Existing results retain old snapshots through reload or unload. Script unload releases its resources automatically. Actual open.mp ordering initializes the new gamemode before component reset, so reset preserves active-script resources and AMX unload performs cleanup. Filterscript and gamemode ownership are independent.

The service is synchronous on the Pawn execution thread. There are no worker threads, asynchronous jobs, IP caches or external services. Concurrency is unsupported. The only reusable performance abstraction is a lookup snapshot, which avoids repeated tree traversals when extracting several fields.

## New Pawn API

The public include remains #include <maxora>; the 1.x MMDB_* API is removed. All 19 natives return an explicit MaxoraStatus, where MAXORA_OK=0. MaxoraDb and MaxoraResult tags distinguish output handles. There is no mutable last-error buffer.

Resource natives: Load, Reload, Unload, IsValid, Lookup, Release. Field natives: GetString, GetText, GetInt, GetFloat, GetBoolean, GetType, GetSize, HasField, GetPrefix. Metadata/diagnostics: GetDatabaseType, GetBuildEpoch, GetIpVersion, GetStatusMessage. Each name has the Maxora_ prefix. Seven stock helpers cover country code, English city name, subdivision code, ASN, organization, latitude and longitude.

Paths use JSON Pointer syntax, including escaped slash/tilde, empty keys, arrays and root selection. Limits are 1,024 UTF-8 bytes and 32 tokens. All supported logical MMDB types can be identified. Integers wider than a Pawn cell are available as exact decimal text; bytes are lowercase hex. Containers can be inspected by type/size and addressed with known paths; unknown map-key enumeration and whole-container serialization are outside the API.

Every native and status is documented in docs/api.md and maxora.inc. Production example and integration scripts compile with the pinned official Pawn 3.10.11 compiler configured explicitly for 32-bit cells.

## Memory safety and security

- Complete native count-plus-argument spans are validated before reading their contents; addresses, alignment, heap/stack regions, references, termination and capacities are checked.
- Output never silently truncates with success. Required string length includes NUL and is returned on BufferTooSmall. Its reference cannot overlap the destination. Packed Pawn strings are rejected; unpacked UTF-8 byte cells are supported.
- Integer narrowing is checked. Floating extraction rejects nonfinite values, overflow and underflow and documents binary 32 rounding. String validation is bounded to 1,048,575 bytes before scanning.
- Numeric IP parsing uses inet_pton and MMDB_lookup_sockaddr, with no DNS. IPv4/IPv6 family errors, missing records, missing fields, bad paths, type mismatches, range errors and decoder failures have separate statuses.
- Mappings close via RAII; snapshots retain their resources. No internal pointer is exposed to Pawn.
- Source review found and fixed argument-block validation and broad IPv6-prefix normalization issues. Regression tests cover both. No additional reportable critical/high issue was found in the bounded review; this is not security certification.

AMX host object/allocation descriptors are trusted runtime state. AMX cannot reveal source-level array bounds, so scripts must pass true capacities. Local database paths are operator-controlled and not filesystem-sandboxed. Files are limited to 1 GiB and structurally validated by libmaxminddb; open is not exhaustive decoding or cryptographic authentication. Mapped files must remain immutable. Deploy a new versioned filename, verify provider integrity separately and reload that file. Maxora does not download or update databases. Provider database licensing remains the operator's responsibility.

## Pinned dependencies and platforms

| Dependency | Version/immutable commit |
| --- | --- |
| libmaxminddb |1.14.1;4a3d17a621c0ebcb81498fd34d313f67fd940ab5 |
| Official open.mp SDK |08cc69a32d96c0eb8fe063d26c78259e54ca853f |
| Official Pawn compiler |3.10.11;29f1a8c7fd2e01929807cd8c50353fbc93bbd651 |
| Synthetic MaxMind-DB fixtures |dc32db5525f9c42ab7006ab3f13c69330b814dd1 |
| Tested open.mp runtime |v1.6.7.3218; archive digests pinned in download_runtime.py |

C++17 and CMake 3.24+ are used. Windows x86 and Linux x86 are deployment targets with actual module-loading tests. Windows local validation used MSVC 19.51.36248.0 and Windows SDK 10.0.26100.0; CI also validates its hosted compiler configurations. Windows/Linux x64 core/native tests are development configurations, not additional distributed component targets. ARM and other architectures are not advertised. Hosted OS images/package updates are recorded but not claimed bit-for-bit pinned.

## Executed validation

The completed pre-candidate workflow passed all seven jobs. Windows and Linux x86 compiled the real modules, executed CTest and loaded those exact build outputs in official open.mp. The runtime harness exercises all 19 natives, IPv4/IPv6, errors and snapshots, filterscript reload/unload/load, stale script-resource rejection across unload and GMX, post-reset reads, periodic reload/unload and graceful shutdown.

Local final CTest passed 1,604,055 core checks and 91 native checks. The generation test crosses the 524,287-generation retirement boundary. Real pinned fixtures cover all logical types,24/28/32-bit trees, broad-prefix IPv6 records containing IPv4, nested maps/arrays and selected corrupt pointers/trees. There are 18 Python release-tool regression tests, all passed locally before the candidate tag.

Linux ASan/UBSan CTest and 30-second mixed stress passed in the completed run. Maxora and libmaxminddb are instrumented. Parser fuzzing completed 7,041,343 inputs in 61 seconds and native-boundary fuzzing 343,015 inputs in 61 seconds without reported crash/sanitizer failure. Fuzzing is bounded evidence, not exhaustive proof. CI preserves raw logs and crash artifacts.

The 60-second uninstrumented CI mixed benchmark completed 10,537,432 measured operations with nine retained resources. Operations include clock-floor and mapping-lifecycle work; this is not an IP-only throughput figure. The instrumented stress completed 1,083,168 mixed operations in 30.000019159 seconds. Sanitizer memory includes instrumentation overhead.

## Performance methodology and actual local results

Eight interleaved workloads cover timer floor, IPv4, IPv6, missing records, three nested fields with temporary and retained results, four databases and transactional mapping lifecycle. Runs warm up 1,000 operations/workload. Latency uses steady_clock; up to 65,536 deterministic reservoir samples are retained per workload. Throughput, mean/p50/p95/p99 and working set are recorded. Allocation counts are explicitly unmeasured. No fair v 1 baseline was established and no speedup over v 1 is claimed.

Five Windows x86 trials measured 1,000 operations per workload per trial. Below are medians of per-trial statistics, not pooled percentiles:

| Workload | Mean Âµs | p50 Âµs | p95 Âµs | p99 Âµs | Timed operations/s |
| --- | ---: | ---: | ---: | ---: | ---: |
| IPv4 lookup/field/release |1.976 |1.800 |4.100 |5.200 |506,124 |
| IPv6 lookup/field/release |3.279 |3.000 |5.400 |7.200 |304,999 |
| Nested 3 fields, temporary result |3.383 |3.200 |6.100 |8.100 |295,639 |
| Nested 3 fields, retained result |1.795 |1.800 |2.500 |3.500 |557,196 |

The retained-result workload was faster in all five local fixture trials, supporting its API rationale. These small, cache-friendly synthetic runs used a host with concurrent engineering activity and are not a production capacity promise or final RC performance qualification. Full ranges, environment, raw files and provenance limitations are in docs/validation/benchmark-review.md.

The paced Windows x86 run completed 60,000 mixed operations in 60.001508 seconds, keeping nine resources. Working set was 5,910,528â†’6,373,376 bytes, with 6,381,568 peak bytes reported. The sample reservoir grows during the run; these snapshots alone do not establish a leak or explain every byte. Real server runs include all server components and have a separate memory profile.

## Release engineering and remaining gates

Separate workflows cover PR validation, compilation/runtime packaging, unit/native tests, sanitizers/fuzzing, performance/stress, candidate publication, stable promotion and optional continuous soak. Actions and source dependencies are pinned. Tests execute automatically. Publication requires all mandatory validation jobs to succeed.

Packages include the validated binary, Pawn include, example, documentation and license notices. SHA-256 files and a per-member manifest bind source commit, version and platform. Verification rejects changed bytes, missing required content, bad paths, mismatched commits/platforms and independently reviewed digest mismatches. Stable promotion does not rebuild: it reuses candidate bytes, checks the successful candidate workflow including artifact tests, and requires commit-bound reviewed gates, package hashes, zero unresolved critical/high problems and at least 86,400 continuous soak seconds. Nonfinite/invalid durations are rejected.

Candidate publication and downloaded-artifact test results, final package digests and continuous soak status will be recorded below when observed. No stable URL is provided because no stable publication occurred.

## Current release/soak status

The candidate workflow completed successfully for commit `c9c5fdd7470f67a93d39afbb3be41c96922eadc3`. Prerelease: https://github.com/itskoleban/Maxora/releases/tag/v2.0.0-rc.1 . Both published ZIP packages were downloaded, verified and their extracted binaries passed actual open.mp integration on Windows and Linux. Packages were downloaded again locally and all manifest members/checksums verified.

| Published package | SHA-256 |
| --- | --- |
| Windows x86 | 94e8b09a6b96ab4ed4dcebfcd5e088942875f1eee54a81ab0efd202dde9fe7b2 |
| Linux x86 | bbfb2d4ee000cf19883f5261a31f76309b3c66d53148e458b854b1730ce6d6fd |

The final continuous 24-hour test is **running, not passed**. It started on November 2 at 16:38:11 America/Bogota (21:38:11 UTC), with the published Windows DLL `2954df7df59969fb99a8bde6b16bca3333413fc492ce6fcf366aef75f7d11112`. It uses the exact candidate harness, pinned runtime archive and synthetic fixtures and samples resident server memory once per second. Normal completion is expected after November 3 at about 16:38 America/Bogota, subject to the machine remaining available. Status is stored in outputs/Maxora-rc1-24h-soak.status.json; final results will be outputs/Maxora-rc1-24h-soak.json. A fresh workload log and live process were verified after startup.

Stable publication remains blocked until that continuous test actually completes, its memory/counters/errors are reviewed, and final production review/authorization is satisfied. The stable gate was executed and correctly rejected incomplete soak/final review. No stable v2.0.0 tag or release exists.

The initial broad automation request was rejected by automatic approval review. After explicit user authorization, a narrower read-only monitor was created: maxora-rc-soak-read-only-monitor. It checks hourly for at most 26 runs and notifies only on completion, failure or needed intervention. It cannot change files/GitHub, restart the test or publish stable. The continuous runtime process remains independent of these checks.

Candidate-specific benchmark review: the final RC pipeline also passed all eight workload integrity checks. On its recorded 4-vCPU AMD EPYC 9V74 Ubuntu x64 core-test runner, the 60-second mixed run completed 10,768,416 measured operations with nine retained resources. The 100,000-operation/workload run recorded IPv4 mean 394.249 ns, p50/p95/p99 381/441/581 ns and IPv6 mean 793.391 ns, p50/p95/p99 772/831/951 ns. Raw data/environment are retained on the development branch in docs/validation/rc1-benchmark/. These are scoped synthetic service measurements and do not supersede the benchmark caveats above.
