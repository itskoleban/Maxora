# Production readiness and validation report

Version: 2.0.0. Current verdict: **NOT VERIFIED FOR STABLE PUBLICATION**.

This report separates implemented validation infrastructure from results actually observed. A passing compiler invocation is not a production verdict. The machine-readable stable gate in `docs/validation/stable.json` intentionally starts unverified and publication fails closed until a maintainer supplies exact-commit evidence and review.

## Mandatory gates

| Gate | Required evidence | Current status |
| --- | --- | --- |
| Windows x86 release build | Compiler/version, full commit, successful workflow | Pending execution/result recording |
| Linux x86 release build | Compiler/version, full commit, successful workflow | Pending execution/result recording |
| Core and native tests | CTest log, exact configuration and fixtures | Pending execution/result recording |
| Actual open.mp integration | Both platform logs, native assertions, gamemode/filterscript lifecycle | Pending execution/result recording |
| ASan and UBSan | Instrumented Maxora and libmaxminddb tests/stress, zero reports | Pending Linux execution |
| Input fuzzing | Recorded libFuzzer run duration/corpus and zero crashes | Pending Linux execution |
| Short stress | Mixed lookup/reload workload, operation/error/resource counters | Pending execution/result recording |
| Performance review | Raw throughput/mean/p50/p95/p99/memory evidence and reproducible environment | Pending execution/result recording |
| Final continuous soak | At least 86,400 seconds, actual RC component, workload/environment/counters/memory | **Not executed; stable blocker** |
| Package verification | ZIP/checksum/internal manifests, required docs/licenses/examples | Pending candidate artifact validation |
| Published RC artifact tests | Download published packages and run actual extracted binaries on both OS | Pending RC publication/tests |
| Security/correctness review | No unresolved critical/high findings, reviewer evidence | Pending final review |
| Final release authorization | Complete evidence review and authorized stable workflow dispatch | Not requested while gates remain incomplete |

## Evidence policy

Record the exact source commit and recursive dependency commits for every run. The Windows and Linux component tests must use the binaries that are packaged. Candidate tests download the published ZIP, validate SHA-256 and manifest, and run the extracted component. Stable publication downloads and verifies the same candidate bytes; it does not rebuild them.

The optional `Continuous 24-hour candidate soak` workflow needs an existing trusted self-hosted Linux runner labelled `maxora-soak`. GitHub-hosted jobs have a six-hour execution limit. Do not represent separate short jobs as one continuous 24-hour process. If no suitable machine is available, preserve the candidate and leave stable blocked; follow the development guide to reproduce the test on existing resources.

`docs/validation/stable.json` requires commit-bound passed records for all gate names defined in `scripts/release_gate.py`, a continuous soak duration of at least 86,400 seconds with environment/workload/evidence, zero unresolved critical/high issues, reviewer identity and review time. A maintainer must inspect evidence contents; nonempty links are not independent proof. Configure the repository's `production` environment with required reviewers where available; this workflow file alone cannot establish that repository setting.

## Known limits

Only synchronous main-thread callers are supported. Database open does not exhaustively decode the entire file. Trusted files must remain immutable while mapped. Database paths are operator-controlled, not sandboxed. Packed Pawn strings are rejected. Pawn integers are signed 32-bit; wider integers require text extraction. UTF-8 output is checked and strings are never silently truncated. Resource/generation budgets are finite and slots retire rather than allow stale-handle reuse. See architecture and API documentation for limits and lifecycle details.

Actual build logs, benchmark figures, commit references, PR links and workflow results must be added as they become available. Unexecuted work must remain labelled pending or blocked.

## Validation observed during implementation

Windows x86 Release built with MSVC 19.51.36248.0 and Windows SDK 10.0.26100.0, with warnings treated as errors. CTest passed core (1,604,055 checks, 4.29s) and pawn_boundary (91 checks, 0.04s). The 18 Python release-tool tests passed. Local official open.mp runtime tests passed both a 60-second initial workload and an expanded 30-second mixed workload, including real script-owned stale-handle cleanup across filterscript reload and GMX. Raw evidence: [mixed runtime](validation/windows-x86-runtime-mixed.json), [initial 60s](validation/windows-x86-runtime.json).

These local runs occurred during working-tree development and are not final RC qualification. Raw benchmark data and five-trial statistics are reviewed in [benchmark-review.md](validation/benchmark-review.md). Bounded source review findings and fixes are in [security-review.md](validation/security-review.md).

GitHub validation for commit 5b6c9005c21bfa0adb2bea734d8192e87c36d0f3 is tracked in [run 41528397618](https://github.com/itskoleban/Maxora/actions/runs/41528397618). Record the completed conclusion and any later candidate run separately. Earlier failed runtime jobs were correctly left failed; the harness/compiler setup was corrected (script paths, portable timer-driven commands, Unix output buffering and explicit 32-bit Pawn compiler cells). Neither those failures nor an in-progress run count as passed gates.

## Completed CI validation before candidate creation

[Run 41528397618](https://github.com/itskoleban/Maxora/actions/runs/41528397618) passed every mandatory job for source commit `5b6c9005c21bfa0adb2bea734d8192e87c36d0f3`. Both Windows x86 and Linux x86 compiled, executed core/native tests, compiled 32-bit-cell Pawn scripts with the pinned compiler, loaded the actual module in official open.mp v1.6.7.3218, proved filterscript/GMX cleanup, ran the mixed workload and shut down cleanly. Both validated builds were packaged successfully.

Linux ASan/UBSan CTest and 30-second mixed stress passed. Parser fuzzing completed 7,041,343 runs in 61 seconds; native-boundary fuzzing completed 343,015 runs in 61 seconds. No sanitizer diagnostic/crash was reported. The uninstrumented 60-second mixed benchmark completed 10,537,432 measured operations with nine retained resources. These operations include a clock-floor workload and mapping lifecycle work and are not all IP lookups. No controlled comparison to v1 was performed. Allocation counts were not instrumented.

Selected raw evidence is retained under [ci-5b6c900](validation/ci-5b6c900/), together with the full downloadable workflow artifacts. Linux RSS/peak values are separate approximate kernel measurements; sanitizer working set includes sanitizer overhead and is not a production-memory estimate. Local benchmark results remain development measurements with the provenance limits stated in their review.

This successful pre-candidate run does not complete the final 24-hour soak or prove the bytes of a future candidate release. The candidate workflow must independently pass, publish explicitly marked prerelease packages, download them and test those extracted binaries. Stable remains blocked.
