# Architecture

Maxora 2.0 has three implementation layers with an explicit dependency direction:

1. `component.cpp` owns the service and Pawn bridge. It resolves the official Pawn component, registers lifecycle events, attaches natives and detaches scripts. It holds non-owning SDK pointers only while their component exists.
2. `pawn_bridge.cpp` validates the AMX boundary, maps an AMX instance to a unique owner identity, invokes the core, and copies checked values to Pawn. A single non-owning callback route is required by the C native ABI; it is set and cleared by the bridge lifetime. It owns no databases and stores no last error.
3. `core.cpp` implements strict IP and JSON Pointer parsers, conversion policy, RAII mappings, resource slots and lookup snapshots. It depends on libmaxminddb and the standard library, with no SDK dependency. Tests can exercise it independently of open.mp.

`Database` closes its MMDB mapping exactly once. Successful open transfers a `shared_ptr<const Database>` into an owned database resource. A lookup contains an immutable mapping reference, a decoder entry and a normalized network prefix. Reload opens a replacement before changing the database resource; old snapshots retain the original mapping. This is lifetime protection, not a promise of concurrent service access. All operations use the Pawn thread.

One table stores database and lookup variants. Positive cell handles encode a slot and generation. Type, generation and owner are checked before access. Invalidation destroys the variant and advances the generation; a slot is permanently retired before its generation could wrap. Clearing a script or component never resets the generation table. The finite lifetime creation limit is documented in the README and specification.

The parsers use bounded storage. IP parsing calls inet_pton and lookup calls MMDB_lookup_sockaddr, avoiding resolver calls. JSON Pointer tokens are decoded into a 1,025-byte local buffer with 33 pointer entries. libmaxminddb handles maps, arrays, pointers and binary decoding. Selected value reads avoid expanding an entire unknown record.

Errors are returned directly as a stable enumeration. Strings borrow mapped bytes only during the native call and are copied to validated unpacked Pawn arrays. Binary and wide-integer text conversions produce bounded owned strings. Integer conversions check signed-cell limits, and floating conversions reject nonfinite, overflowing or underflowing results. No raw decoder pointer crosses the Pawn boundary.

Script unload invalidates all resources owned by that AMX identity. On GMX, open.mp can initialize the new gamemode before calling component reset; reset therefore preserves active scripts. The outgoing AMX unload event performs cleanup. Filterscripts remain independently owned. Component free or Pawn-component removal detaches callbacks and releases all resources.

Reusable result handles are intentionally the only performance abstraction. They let country, city, ASN or other fields share a tree traversal; benchmarks compare ephemeral and retained lookups. There is no IP cache, background job system or asynchronous API. Performance claims must refer to measured fixture, build, hardware and workload evidence.

The normative bounds, status rules, supported values and acceptance conditions are in [the functional specification](specification.md). The [readiness report](production-readiness.md) separates implemented contracts from actual validation results.
