# Building and testing

Use CMake 3.24 or newer and a C++20 toolchain. Clone the repository with recursive submodules or run `git submodule update --init --recursive` in an existing clone.

## Windows x86

~~~powershell
cmake -S . -B build -A Win32
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix stage
~~~

## Linux x86

Install the 32-bit C and C++ development libraries. The CI configuration uses GCC multilib and Ninja.

~~~sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 \
  -DCMAKE_EXE_LINKER_FLAGS=-m32 -DCMAKE_MODULE_LINKER_FLAGS=-m32
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix stage
~~~

The stage contains the component under `components/` and the Pawn header under `include/`. Use `-DMMDB_BUILD_COMPONENT=OFF` for host builds that exercise the C++ core and Pawn memory boundary without compiling the x86 component.

## Sanitizers and fuzzing

For Linux Clang, use `-DMMDB_SANITIZERS=ON -DMMDB_BUILD_FUZZERS=ON -DMMDB_BUILD_COMPONENT=OFF`, then run CTest and the `omp_mmdb_parser_fuzz` and `omp_mmdb_native_fuzz` targets with copies of the seed corpora. Use `-DMMDB_THREAD_SANITIZER=ON` in a separate build. ThreadSanitizer covers concurrent immutable snapshots during replacement.

## Real-server lifecycle integration

The [build workflow](../.github/workflows/build.yml) compiles the pinned Pawn compiler and uses the [integration runner](../scripts/openmp_integration.py) to compile the example and run the packaged component under real open.mp. [Fixture provenance](../tests/FIXTURES.md).

The integration records MMDB status codes inside the exit callbacks for filterscript reload and ordinary unload, and for `gmx`. It checks that replacement scripts start with empty namespaces and that the server exits normally. The store test checks that detaching one script releases its mapping and that clearing the service releases the remaining mappings when no snapshot owns them.

With the digest-verified open.mp v1.6.7.3218 archive, the runner also checks that `mmdb_is_loaded` and `mmdb_get_ip_version` return `MMDB_INVALID_ARGUMENT` in both callbacks during full server shutdown. This is an observation for that archive. The [public contract](api.md#database-management) leaves native availability during full shutdown unspecified. Runs with `--server-dir` skip the final native probes and still require a clean exit.
