# Installation

Maxora performs local, read-only MMDB lookups. It does not download, update, authenticate or license databases.

Use release candidate packages for evaluation until the stable gates in [production-readiness.md](production-readiness.md) have passed. Windows x86 and Linux x86 are the baseline deployment targets under validation; do not assume a successful core-only x64 test establishes server binary compatibility.

1. Obtain the matching platform ZIP and its `.sha256` file from the same release. Verify the SHA-256 checksum before extraction.
2. Stop the server before replacing its component binary. Copy `components/Maxora.dll` or `components/Maxora.so` into the server's `components` directory.
3. Copy `include/maxora.inc` into the Pawn compiler include directory and recompile scripts with `#include <maxora>`.
4. Obtain a legally licensed MMDB edition and place it at an operator-controlled local path. Paths resolve relative to the server working directory unless absolute paths are supplied. Follow the API reference for limits and ownership.
5. Start open.mp, inspect the component log and execute a known-record smoke lookup. Treat missing fields as edition-dependent results.

Use [MaxMind GeoIP Update](https://dev.maxmind.com/geoip/updating-databases/) or an operator-managed update procedure appropriate to your license. Download and validate a new file separately, then load/reload from a new filename. Do not overwrite or truncate a file mapped by a running process; this can cause operating-system faults. Retain the old file until all snapshot lookup results are released. Review database terms, attribution and redistribution restrictions; Maxora's MIT license does not grant rights to third-party databases.

On Windows, mapped files may be locked against replacement. On Linux, a rename-based deployment can preserve existing mappings, but using a distinct versioned filename is portable. Recompile all v1 scripts: the v2 Pawn API is intentionally incompatible.
