# omp-mmdb 0.1.0

omp-mmdb replaces Maxora's Pawn interfaces with case-sensitive database names scoped to each script. Queries take an IP and field path directly, with no persistent result objects to release. The component is distributed as `MMDB.dll` or `MMDB.so` and scripts include `<omp_mmdb>`.

This is an API break from both Maxora 1.x and Maxora 2.0 RC. See the [migration guide](migration.md) for changes to source and deployment. Reload is transactional and synchronous. [Production qualification](production-readiness.md) describes the evidence required for stable publication.
