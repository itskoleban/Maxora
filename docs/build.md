# Build instructions

Maxora 2.0.0 uses C++17 and CMake 3.24 or newer. Dependencies are Git submodules pinned by commit. Clone with `git clone --recursive https://github.com/itskoleban/Maxora.git` and select the v2 development branch or release candidate explicitly. Run `git submodule update --init --recursive` after changing revisions.

The deployment targets under validation are Windows x86 and Linux x86. A 64-bit core-only build is useful for parser tests, sanitizers and benchmarks; it is not a distributable open.mp component. ARM and other deployment architectures are not supported by this release pipeline.

## Windows x86

Install Visual Studio with the C++ desktop workload and a Windows SDK. The upstream SDK documents Visual Studio 2019 or newer; this project's CI selects the image-provided current Visual Studio and Win32 generator. Runtime compatibility is established by the actual CI server integration result, not that minimum alone.

```powershell
cmake -S . -B build -A Win32 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix stage
python scripts/package.py --stage stage --platform windows-x86 --out dist
```

## Linux x86

Install CMake, Ninja, GCC, `gcc-multilib`, `g++-multilib` and Python 3. The binary also requires the corresponding 32-bit C/C++ runtime and the runtime dependencies of the selected open.mp server.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 \
  -DCMAKE_EXE_LINKER_FLAGS=-m32 -DCMAKE_MODULE_LINKER_FLAGS=-m32
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix stage
python3 scripts/package.py --stage stage --platform linux-x86 --out dist
```

Maxora uses compiler warnings as errors by default. Third-party code retains its upstream diagnostics. The CMake 3.5 policy compatibility floor permits older SDK dependency build files under CMake 4; it does not suppress compiler diagnostics. Production compiler and runner package versions should be recorded with each validation run; hosted runner images and OS packages are not claimed to be bit-for-bit pinned.

## Core-only development build

```sh
cmake -S . -B build-core -DMAXORA_BUILD_COMPONENT=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --config Release --parallel
ctest --test-dir build-core -C Release --output-on-failure
```

`MAXORA_BUILD_TESTS` and `MAXORA_BUILD_BENCHMARKS` default to ON. `MAXORA_SANITIZERS=ON` requires Linux GCC or Clang. `MAXORA_BUILD_FUZZERS=ON` requires Linux Clang. Do not use sanitizer or fuzzer binaries as release artifacts.
