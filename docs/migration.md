# Migration from Maxora 1.x

Version 2.0.0 is a new implementation with a new API. The v1 source, tags and releases remain in Git history. Do not mix `maxora.inc` from v1 and a v2 component.

Replace `MMDB_*` calls with the `Maxora_*` interface documented in [api.md](api.md). All fallible operations return an explicit status; success is zero. Database and lookup handles have distinct Pawn tags. Open a database into an output reference, perform one lookup and extract multiple fields from its result handle, then release that result. Paths use JSON Pointer (`/country/iso_code`) instead of dotted paths. Escape `~` as `~0` and `/` inside keys as `~1`.

Resources are script-owned. Another gamemode or filterscript cannot use or unload the owner's handles. Script unload releases its resources; server reset invalidates all resources. Store each handle only while its owner remains active. Unloading a database invalidates its table handle. Reload retains the handle and switches future lookups to the replacement; existing lookup snapshots retain their original database safely until released. Review the reload contract in the API reference.

Remove the global last-error pattern. Check each native's returned status directly; no unrelated script can overwrite that result. Distinguish no record, no field, wrong type, small output buffer and numeric range failure. Strings must fit with their terminator; retry with a larger Pawn array after `MAXORA_BUFFER_TOO_SMALL`, and never treat partially filled output as successful.

Review application behavior for IPv6, missing database fields, and full-range ASN values that exceed signed 32-bit Pawn cells. Use textual integer extraction for values outside the Pawn integer range. The component does not automatically fall back between unrelated MMDB editions or acquire replacement databases.

