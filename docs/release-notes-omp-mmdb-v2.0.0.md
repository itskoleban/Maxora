# omp-mmdb 2.0.0 release notes

omp-mmdb 2.0.0 is the incompatible successor to Maxora 1.x and the Maxora 2.0 candidate. It is designed for scalable open.mp servers. Its redesigned Pawn API uses case-sensitive database names scoped to each script. Queries take an IP address and JSON Pointer directly. The public API exposes no database or result handles. Reload is transactional and synchronous.

The Windows x86 package contains `MMDB.dll`. The Linux x86 package contains `MMDB.so`. Both provide `include/omp_mmdb.inc`. Recompile affected scripts with `<omp_mmdb>` and follow the [migration guide](migration.md) for source and deployment changes.

omp-mmdb candidates use `v2.0.0-omp-mmdb.rc.N` tags. The historical Maxora candidate retains `v2.0.0-rc.1`. Stable publication under `v2.0.0` uses the verified candidate packages after all [production qualification gates](production-readiness.md) pass.
