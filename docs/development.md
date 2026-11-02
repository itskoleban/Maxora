# Development and testing

Tests execute with CTest; building test executables alone is not validation. `core` exercises the real engine using redistributable synthetic MMDB fixtures from the pinned MaxMind-DB submodule. `pawn_boundary` exercises native boundaries in component and core-only builds. The external integration harness compiles Pawn scripts and loads the actual component in the pinned open.mp server. See workflow logs and the production-readiness report for results that were actually observed.

```sh
ctest --test-dir build -C Release --output-on-failure
```

Use a clean checkout for reproducible validation. Record the full source commit, recursive submodule commits, compiler version, operating system and complete commands. Tests must fail on assertion failures or nonzero process status. The old `test/test_backend.cpp` is historical source in v1 tags and is not part of the v2 test suite.

## Sanitizers and fuzzing

```sh
CC=clang CXX=clang++ cmake -S . -B build-sanitize -G Ninja \
  -DMAXORA_BUILD_COMPONENT=OFF -DMAXORA_SANITIZERS=ON \
  -DMAXORA_BUILD_FUZZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-sanitize
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize --output-on-failure
mkdir -p fuzz-corpus
cp fuzz/corpus/* fuzz-corpus/
build-sanitize/maxora_parser_fuzz fuzz-corpus -max_total_time=60 -max_len=4096
mkdir -p native-corpus
cp fuzz/native-corpus/* native-corpus/
build-sanitize/maxora_native_fuzz native-corpus -max_total_time=60 -max_len=4096
```

The parser harness targets numeric IP parsing, JSON Pointer parsing and UTF-8 validation. The native harness exercises fourteen read-only natives against a fixed synthetic database, mutating AMX counts, spans, paths, capacities and cell contents; it never opens fuzz-controlled file paths. Core and native boundary code have libFuzzer coverage instrumentation. This complements upstream decoder tests and fuzzing; it is not proof that every corrupted file is rejected during open. MMDB record decoding is lazy. A database may open successfully and fail when a damaged entry is reached.

## Runtime integration

Build the pinned Pawn compiler separately:

```sh
cmake -S lib/compiler/source/compiler -B build-pawn -DCMAKE_C_FLAGS=-DPAWN_CELL_SIZE=32 -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release
cmake --build build-pawn --config Release
```

Run `python scripts/openmp_integration.py --help` for the current harness arguments. The harness requires an extracted supported server and the matching x86 component. It must observe explicit success markers, including gamemode/filterscript lifecycle checks. A process starting successfully does not establish native behavior.

## Stress and final soak

First complete unit, boundary, real runtime, sanitizer, fuzz and short stress validation. Then run one continuous 24-hour workload against the exact release candidate artifact on an existing dedicated test machine. The soak must record monotonic duration, full source commit, package and binary SHA-256, server version, compiler/build provenance, fixture hashes, workload, completed operations, failures and memory samples. Include repeated IPv4/IPv6 queries, missing values, multiple databases and periodic reload/unload/reload cycles. Check stable memory and zero unexpected failures.

GitHub-hosted Actions jobs have a six-hour limit, so a continuous 24-hour soak cannot honestly be represented by one hosted job or stitched short jobs. The optional soak workflow targets an existing self-hosted runner labelled `maxora-soak`; do not register a privileged personal machine for untrusted pull requests. No paid infrastructure is required by this repository. A missing runner or incomplete duration is a blocker, never a pass.
