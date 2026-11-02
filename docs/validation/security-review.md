# Bounded security and correctness review

Reviewed implementation: new v2 core, resource table, Pawn bridge, component lifecycle and typed public API. Review performed during development on 2026-11-02; final candidate identity and automated evidence must be checked separately. This is a source review with regression tests, not a certification or proof of absence of vulnerabilities.

## Issues found and fixed

1. The native dispatcher initially validated the argument count but trusted the runtime-supplied parameter block extent. The final bridge validates the complete expected count-plus-arguments span inside allocated AMX memory before reading the count. Tests cover foreign, misaligned, heap-gap and truncated blocks. The native fuzzer mutates these boundaries while keeping host allocations valid.
2. IPv4 queries in a valid IPv6 database can match a record whose prefix is shorter than the 96-bit embedded IPv4 offset. The initial normalization incorrectly treated that as corruption. The final conversion clamps those matches to IPv4 /0 and subtracts 96 otherwise. Regression tests exercise both 1.1.1.1 and 255.255.255.255 using upstream's MaxMind-DB-no-ipv4-search-tree.mmdb.
3. String validation is bounded to 1,048,575 source bytes before UTF-8 scanning. A regression case uses a deliberately oversized declared length backed by a one-byte buffer to prove the bound is checked before the scan. Larger values return OUT_OF_RANGE; no truncation is reported as success.

## Reviewed mechanisms

- Database mappings have exactly one RAII close operation. Shared immutable mapping references keep lookup snapshots alive after database handle reload/unload.
- One variant table validates resource kind, owner and generation. Invalidated generations never restart; exhausted slots retire. Tests cross the 524,287-generation retirement boundary and preserve stale-handle rejection.
- Per-AMX owner identities are monotonic; unload releases all resources. Real server tests verify invalid old handles across filterscript reload/unload/load and GMX. Reset preserves active-script resources because of verified open.mp ordering.
- Numeric parsing uses inet_pton and MMDB_lookup_sockaddr without resolver calls. Paths have bounded storage and strict JSON Pointer escape/UTF-8 validation.
- String capacities, output references, alignment, heap/stack allocated regions, termination, integer ranges, float finiteness/range and type mismatches are checked. A required-length reference cannot overlap its string destination.
- C++ failures are caught at the native callback and component entry boundaries. No internal pointers or mutable last-error state cross into Pawn.

## Trust boundaries and residual limits

AMX runtime object pointers and allocation descriptors come from the trusted server. Maxora validates Pawn-controlled addresses and cells inside those allocations; it cannot recover from a corrupted host AMX object or discover source-level Pawn array declarations. Callers must pass their actual buffer capacities. Scripts can open operator-controlled local file paths; this is not a filesystem sandbox.

Only synchronous Pawn-thread execution is supported. Database mappings must stay immutable. libmaxminddb validates accessed structure and lazily decoded values, not provider authenticity or every record at load. A mapped file truncated externally can produce an OS fault. Use new versioned filenames for updates, especially on Windows, and keep old files until snapshots release them.

Handle issuance is finite because positive 32-bit handles cannot provide an unlimited never-reused domain. Limits and retirement behavior are explicit. Resource quotas do not impose a database-byte memory quota. Native string marshalling and wide-value text conversion allocate bounded memory; no zero-allocation, lock-free or thread-safe claim is made.

No additional reportable critical/high issue was identified in this bounded review after the fixes. Stable publication still requires exact-candidate CI, artifact tests, soak evidence and final review; this statement does not satisfy those gates by itself.
