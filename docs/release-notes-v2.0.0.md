# Maxora 2.0.0 release notes

Maxora 2.0.0 is a ground-up replacement of the local MMDB component for open.mp. It introduces script-owned database and lookup handles, transactionally replaced database mappings, strict numeric IP parsing, bounded JSON Pointer field paths and explicit status reporting. A lookup result can retrieve multiple fields while retaining its original mapping across reload/unload. The public interface remains `#include <maxora>`; the `MMDB_*` API is removed.

The engine is pinned libmaxminddb 1.14.1, with the open.mp SDK pinned by commit. Database downloads and automatic updates remain operator responsibilities. Third-party database license terms continue to apply.

Baseline packages target Windows x86 and Linux x86. Treat each candidate as evaluation software until all mandatory gates in the production-readiness report are passed, including actual artifact loading on both operating systems and a continuous 24-hour final soak. Core-only x64 builds support development validation and are not component deployment packages.

Read the installation, API and migration guides before upgrading. Recompile every Pawn script using the supplied v2 include. Check every status explicitly: success is zero. Ensure results are released and configure database updates with distinct filenames; do not truncate mapped files.

Each package contains the component, Pawn include, documentation, original MIT license and dependency notices. Verify the accompanying SHA-256 checksum and internal manifest before installation. Stable publication promotes the exact validated candidate package bytes and uses tag `v2.0.0` only after every gate is demonstrably passed.

Known release limitations and the actual test/benchmark evidence are recorded in [production-readiness.md](production-readiness.md). No stable release is claimed by the presence of these notes.
