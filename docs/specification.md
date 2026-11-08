# Maxora 2.0 functional specification

Version: 2.0.0. Status: implementation under validation; not a stable release declaration.

## Product boundary

Maxora is an open.mp x86 component for synchronous local read-only MMDB queries from Pawn. Windows and Linux x86 are release targets, contingent on build and runtime evidence. Core-only x64 test builds are development tools. The component does not download databases, resolve hostnames, perform network I/O, spawn threads, or cache IP results. It uses the supported libmaxminddb decoder, not a custom decoder.

## Ownership and lifetime

Each loaded Pawn script receives a distinct component-local owner identity. Every database and reusable lookup belongs to that script. Other scripts cannot use, mutate, or release its resources. Script unload releases all its resources, even if its exit callback omitted cleanup. Component shutdown releases all remaining resources. In the supported open.mp lifecycle, reset preserves resources of active scripts because the new gamemode can initialize before reset; unload events clean the outgoing script.

A handle is a positive signed 32-bit cell containing a 12-bit slot and a 19-bit generation. Zero is invalid. Slots increment generation whenever invalidated and are permanently retired at generation exhaustion. The table itself persists for the component lifetime; clearing it must not restart generations. Database and lookup resources occupy one table and type checks prevent cross-kind access. A stale handle cannot become valid again within a component lifetime. A newly started server is a new handle domain; scripts must never persist handles outside their process.

Limits: 4,095 slots globally; 64 live database handles and 256 live lookup handles per script. Retained lookups also retain their database mapping, so these limits are not a memory-byte quota. Scripts should release results promptly. Operations are supported only on the server's Pawn execution thread; concurrent service callers are outside the contract.

## Database operations

Load accepts a nonempty path of at most 4,096 UTF-8 bytes naming a regular readable file of at most 1 GiB. libmaxminddb validates the file structure and metadata it reads. Maxora additionally requires format major 2, IP version 4 or 6, and tree record size 24, 28, or 32. This is structural validation, not exhaustive validation or cryptographic file authentication. Decoder errors encountered later remain explicit failures. File authenticity and licensing remain operator responsibilities.

Reload accepts an existing owned database handle and another path. Failure leaves the original database intact. Success preserves the database handle and directs future lookups at the replacement. Existing lookup handles remain snapshots of the original mapping. Unload invalidates the database handle; snapshots remain valid until explicitly released or their script unloads. Mapped files must remain immutable; deploy updates as new files and reload to the new path.

Metadata exposes database type, IP version, and build epoch (decimal text avoids a 2038 signed-cell limit). Missing, unreadable, empty, oversized, corrupt and unsupported databases fail without creating a resource.

## Queries and generic values

Lookup strictly accepts numeric IPv4 or IPv6. Hostnames, abbreviated IPv4, whitespace, port suffixes, CIDR suffixes, zone IDs, embedded NULs and invalid encodings fail. Parsing uses inet_pton; lookup uses MMDB_lookup_sockaddr. No resolver is called. IPv6 in an IPv4 database is an unsupported-family error. A valid IP without a record is distinct from a record without a requested field. IPv4 prefixes queried in IPv6 databases are normalized to IPv4 prefix lengths.

A successful lookup creates an owned reusable result. Several field extractions can share one tree traversal. Paths use JSON Pointer syntax: empty string selects the record root, `/country/iso_code` selects nested keys, `/subdivisions/0/iso_code` selects arrays, `~0` escapes tilde and `~1` escapes slash. Empty keys are supported. The parser rejects invalid escapes, embedded NUL, invalid UTF-8, more than 1,024 bytes or more than 32 tokens. Array indexing follows libmaxminddb (including negative offsets); decoder path errors remain explicit.

Supported logical MMDB types are UTF-8 string, byte string, uint16/32/64/128, int32, float, double, Boolean, map and array. Type introspection works for containers. Map keys can be addressed by generic paths; this API does not enumerate unknown keys. String extraction requires valid UTF-8 with no embedded NUL. Integer extraction accepts integer types only and checks signed 32-bit cell range. Float extraction accepts float/double only, rejects nonfinite values and overflow/underflow, and rounds to Pawn's binary32 precision. Boolean extraction requires a Boolean. Text extraction represents all integer widths as decimal and bytes as lowercase hexadecimal; it does not stringify containers. Byte expansion is bounded to 32,767 source bytes.

## Pawn boundary and errors

The public interface is `#include <maxora>`. Database and lookup handles have distinct Pawn tags. Every native returns an explicit status; no process-global or thread-local last-error state is used. Error message lookup is a pure mapping from status code. Output references are modified only according to the documented success/required-size contract. String output never silently truncates with success. Destination capacity is measured in Pawn cells, includes the terminator, and is bounded. AMX arguments, counts, alignment, readable ranges, references, writable ranges and termination are checked before accessing memory. Runtime cannot discover an array declaration's logical size, so scripts must pass its true capacity; checks protect allocated AMX regions, not source-level adjacent variables.

## Acceptance evidence

CTest executes parser, conversion, ownership, generation, lifecycle, corruption, real fixture and native-boundary cases. Runtime smoke tests must load the actual module into official Windows/Linux open.mp x86 and execute compiled Pawn scripts. Sanitizers instrument both core and database library on supported Linux configurations. Fuzzing targets parsers and untrusted byte boundaries. Benchmarks record environment, pinned fixtures, workloads, throughput, mean/p50/p95/p99 and measured memory where available. No faster-than-v1 or zero-allocation assertion is made without a fair experiment.

Stable publication requires all configured platform, native/runtime, sanitizer, fuzz, stress, package and checksum checks, review of unresolved defects, and a continuous 24-hour soak of the actual candidate with documented environment and results. A six-hour hosted job cannot count as a 24-hour continuous soak. Unexecuted gates remain blockers. See the production validation report for current evidence.

Each slot retires before generation wrap. The maximum lifetime issuance budget is 4,095 × 524,287 successful resource creations. Exhaustion returns ResourceLimit; no claim of unlimited process uptime under arbitrary handle churn is made. String extraction rejects more than 1,048,575 source bytes before scanning, independently of the database size.
