# Changelog

## omp-mmdb 2.0.0 — pending release

The redesign used `0.1.0` during development. omp-mmdb 2.0.0 is incompatible with Maxora 1.x and the Maxora 2.0 candidate. [Release notes](docs/release-notes-omp-mmdb-v2.0.0.md)

- Install the open.mp component as `MMDB.dll` or `MMDB.so` and use `#include <omp_mmdb>`.
- Replace public handles with script-local database names and direct IP queries.
- Preserve transactional reload and release each script's mappings at AMX unload.
- Validate MMDB sections and traverse maps and arrays with type-aware JSON Pointer tokens.
- Use the reader's mapping ceiling in place of the 1 GiB policy limit.
- Add namespace and snapshot stress coverage and compare the new API with the Maxora 2.0 baseline.

## Maxora 2.0.0-rc.1 — historical prerelease

- Replace the legacy implementation with a C++17 service, RAII database ownership, script-owned generational resources and database snapshot lookup results.
- Introduce an intentionally incompatible `Maxora_*` Pawn API with tagged database/result handles and explicit status codes.
- Add strict numeric IPv4/IPv6 input, bounded JSON Pointer paths, generic MMDB type inspection and checked string/integer/float/boolean/text extraction.
- Add transactional reload, independent databases, explicit result release, script unload cleanup and server reset invalidation.
- Add executable CTest suites, native boundary tests, real open.mp integration harness, sanitizer/fuzzer configurations and reproducible benchmarks.
- Replace build-only publication with separate validation, candidate, artifact testing and fail-closed stable release gates; package documentation/licenses and SHA-256 manifests.

The historical `v2.0.0-rc.1` tag retains the original Maxora commit. The validation record documents the original release assets. [Maxora release notes](docs/release-notes-v2.0.0.md) · [Tagged source](https://github.com/itskoleban/omp-mmdb/tree/v2.0.0-rc.1) · [Validation record](https://github.com/itskoleban/omp-mmdb/blob/4895916c055fb1a3da3173c6c7c637b3b3fee068/docs/validation/candidate-rc1.json)

## 1.1.1 — 2026-07-24

Historical API documentation release. Original source and binary assets remain at tag `v1.1.1`.

## 1.1.0 — 2026-07-24

Historical multi-database release. Original source and assets remain at tag `v1.1.0`.

## 1.0.0 — 2026-07-18

Historical initial tagged release. Original source and assets remain at tag `v1.0.0`.
