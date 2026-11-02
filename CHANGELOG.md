# Changelog

## 2.0.0 — release candidate under validation

- Replace the legacy implementation with a C++17 service, RAII database ownership, script-owned generational resources and database snapshot lookup results.
- Introduce an intentionally incompatible `Maxora_*` Pawn API with tagged database/result handles and explicit status codes.
- Add strict numeric IPv4/IPv6 input, bounded JSON Pointer paths, generic MMDB type inspection and checked string/integer/float/boolean/text extraction.
- Add transactional reload, independent databases, explicit result release, script unload cleanup and server reset invalidation.
- Add executable CTest suites, native boundary tests, real open.mp integration harness, sanitizer/fuzzer configurations and reproducible benchmarks.
- Replace build-only publication with separate validation, candidate, artifact testing and fail-closed stable release gates; package documentation/licenses and SHA-256 manifests.

Stable publication remains gated by the production validation report. No unsupported platform, throughput or full-file-integrity claim is implied by this entry.

## 1.1.1 — 2026-07-24

Historical API documentation release. Original source and binary assets remain at tag `v1.1.1`.

## 1.1.0 — 2026-07-24

Historical multi-database release. Original source and assets remain at tag `v1.1.0`.

## 1.0.0 — 2026-07-18

Historical initial tagged release. Original source and assets remain at tag `v1.0.0`.
