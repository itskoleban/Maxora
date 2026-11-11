# Production readiness

The redesign requires fresh validation on the exact omp-mmdb release candidate. Historical Maxora artifacts and records under `docs/validation/` describe prior source and packages.

Before stable publication:

1. Review successful Windows x86 and Linux x86 builds, packaged open.mp integration, Pawn compilation and cleanup tests for the candidate commit.
2. Review ASan, UBSan, TSan, parser and native fuzz results. Check repeated loads, failed replacements and concurrent reads of immutable snapshots.
3. Validate legally obtained provider files, including representative large databases on x86. Record page-cache conditions, percentile latency, RSS and the effect of keeping two mappings during reload.
4. Run a continuous 24-hour open.mp soak with the exact packaged candidate, record the logs and attach verified package hashes and workflow evidence.
5. Obtain explicit stable publication authorization for the verified candidate. See the [candidate](../.github/workflows/candidate.yml), [soak](../.github/workflows/soak.yml) and [stable](../.github/workflows/stable.yml) workflows.
