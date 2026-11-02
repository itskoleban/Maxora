# Maxora 2.0

Maxora is an open.mp component for local, read-only MaxMind DB lookups from Pawn. Version 2.0.0 is a new implementation with a new API. Its production validation is tracked in [the readiness report](docs/production-readiness.md); source availability does not establish a stable release.

## Capabilities

- Independent script-owned databases, explicit unload, and transactional reload.
- IPv4 and IPv6 numeric parsing without DNS or external services.
- Reusable lookup snapshots, generic nested map/array paths, and type inspection.
- Checked string, signed-cell integer, binary32 float, Boolean and wide-integer/byte text extraction.
- Handles with generations, script unload cleanup, bounded resources and explicit status codes.
- CTest, native-boundary tests, real open.mp smoke tests, parser fuzzing, sanitizers and reproducible benchmarks.

The intended deployment targets are **Windows x86 and Linux x86**, tested against open.mp **v1.6.7.3218**. See the readiness report for what actually passed on the candidate. Core-only x64 builds support development; they are not additional component distribution targets.

## Pawn usage

```pawn
#include <maxora>

new MaxoraDb:database;

main()
{
    if (Maxora_Load("scriptfiles/GeoLite2-Country.mmdb", database) != MAXORA_OK)
        return;

    new MaxoraResult:result;
    if (Maxora_Lookup(database, "8.8.8.8", result) == MAXORA_OK)
    {
        new country[3], required;
        // Country availability depends on the database edition and record.
        Maxora_GetString(result, "/country/iso_code", country, sizeof(country), required);
        Maxora_Release(result);
    }
    Maxora_Unload(database);
}
```

Every native returns a status. `MAXORA_OK` is **zero**; do not test success as a truthy value. Handles are outputs, not native return values. The public include remains `#include <maxora>`. The old `MMDB_*` API is intentionally absent. See [the full API reference](docs/api.md) and [compiled examples](examples/).

## Install and build

Install a validated package's `components/Maxora.dll` or `components/Maxora.so` into the server components directory and `include/maxora.inc` into your Pawn compiler include path. A database must be supplied separately. Maxora neither downloads nor updates databases. Follow the provider's update procedure and license terms.

```sh
git clone --recursive https://github.com/itskoleban/Maxora.git
cd Maxora
cmake -S . -B build -A Win32
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Use the rewrite branch until it is reviewed and merged. See [build instructions](docs/build.md) for Linux x86 and core-only builds. Dependencies are pinned Git submodules: libmaxminddb **1.14.1**, the official open.mp SDK, and the official Pawn compiler. Preserve the nested submodule pins; do not replace them with moving branch heads.

## Documentation

- [Installation and updates](docs/installation.md)
- [Pawn API and error reference](docs/api.md)
- [Architecture and functional specification](docs/architecture.md), [testable contracts](docs/specification.md)
- [Build](docs/build.md), [development and testing](docs/development.md)
- [Performance and soak methodology](docs/performance.md)
- [Troubleshooting](docs/troubleshooting.md), [security considerations](docs/security.md)
- [Migration from 1.x](docs/migration.md)
- [Changelog](CHANGELOG.md), [2.0.0 release notes](docs/release-notes-v2.0.0.md)
- [Production readiness and validation](docs/production-readiness.md)

## Limits and deployment responsibilities

Calls run synchronously on the Pawn execution thread. Concurrent callers are unsupported. Each script may own 64 database handles and 256 lookup handles; the component has 4,095 resource slots. Paths are bounded to 1,024 bytes and 32 tokens. Files are limited to 1 GiB. Keep mapped database files immutable and reload a newly deployed file instead of overwriting an active mapping. Old snapshots retain old mappings until released.

Generations retire before wrapping, so stale handles never alias new resources within a process. The 31-bit handle domain is finite: at most 4,095 × 524,287 successful resource creations before all slots retire. Resource-limit errors are explicit. Release results promptly and plan process restarts for workloads that approach this bound. No claim of unlimited lifetime, arbitrary database trust, zero allocations, or speed relative to 1.x is made.

## License and history

Maxora retains its [MIT license](LICENSE), copyright, Git history and v1.x release tags. The rewrite replaces implementation files without rewriting public history. Packages include dependency licenses and notices. Synthetic tests use the pinned MaxMind-DB fixtures under their redistributable license; no commercial or GeoLite database is committed.

