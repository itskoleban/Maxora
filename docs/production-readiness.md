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
