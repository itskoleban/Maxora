# Production readiness

For omp-mmdb 2.0.0, use candidate tags of the form `v2.0.0-omp-mmdb.rc.N`, where `N` is a positive integer without leading zeros. The first candidate uses `v2.0.0-omp-mmdb.rc.1`. The historical `v2.0.0-rc.1` tag identifies the Maxora candidate. The records under `docs/validation/` describe the earlier source and packages. [Maxora candidate source](https://github.com/itskoleban/omp-mmdb/tree/v2.0.0-rc.1)

Dispatch the [candidate workflow](../.github/workflows/candidate.yml) on the existing omp-mmdb candidate tag with `publish v2.0.0 candidate`. Candidate packages use version `2.0.0` in ZIP filenames and manifests. Stable publication under `v2.0.0` promotes the exact verified candidate ZIP and checksum files.

Before stable publication:

1. Review successful Windows x86 and Linux x86 builds, packaged open.mp integration, Pawn compilation and cleanup tests for the candidate commit.
2. Review ASan, UBSan, TSan, parser and native fuzz results. Check repeated loads, failed replacements and concurrent reads of immutable snapshots.
3. Validate legally obtained provider files, including representative large databases on x86. Record page-cache conditions, percentile latency, RSS and the effect of keeping two mappings during reload.
4. Run the [continuous 24-hour open.mp soak](../.github/workflows/soak.yml) with the exact packaged candidate, record the logs and attach verified package hashes and workflow evidence.
5. After reviewing the evidence for the exact candidate tag and commit, dispatch the [stable workflow](../.github/workflows/stable.yml) with that tag and `publish verified v2.0.0`.

If stable publication fails after creating `v2.0.0`, check the tag's commit and the release state before completing publication manually with the reviewed assets and evidence. A workflow retry stops when the stable tag already exists.
