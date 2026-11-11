# omp-mmdb 2.0.0 release notes

omp-mmdb 2.0.0 is the incompatible successor to Maxora 1.x and the Maxora 2.0 candidate. It is designed for scalable open.mp servers. Its redesigned Pawn API uses case-sensitive database names scoped to each script. Queries take an IP address and JSON Pointer directly. The public API exposes no database or result handles. Reload is transactional and synchronous.

The Windows x86 package contains `MMDB.dll`. The Linux x86 package contains `MMDB.so`. Both provide `include/omp_mmdb.inc`. Recompile affected scripts with `<omp_mmdb>` and follow the [migration guide](migration.md) for source and deployment changes.

omp-mmdb release candidates begin at `v2.0.0-rc.1`, followed by `v2.0.0-rc.2` if needed. The prior Maxora tag named `v2.0.0-rc.1` was deleted before starting this sequence; the prior source history and validation notes remain available. Stable publication under `v2.0.0` uses the verified candidate packages after all [production qualification gates](production-readiness.md) pass.
