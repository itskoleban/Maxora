# Pawn API reference

Include `<omp_mmdb>`. All natives return `MMDBStatus:`. The [public include](../include/omp_mmdb.inc) is the source for complete Pawn declarations.

## Database management

| Native | Parameters | Result on success |
|---|---|---|
| `mmdb_load` | `name, file` | Open a local file under a new name |
| `mmdb_reload` | `name, file` | Open and validate a replacement before swapping it in |
| `mmdb_unload` | `name` | Release the named database |
| `mmdb_is_loaded` | `name, &bool:loaded` | Write whether this script has that name |

Names contain 1–63 ASCII letters, digits, underscores, hyphens or dots. Names are case-sensitive. File paths are unpacked UTF-8 strings of 1–4,095 bytes, relative to the server working directory or absolute. Use server-controlled filenames. Each script can open up to 64 databases at a time, with a component-wide limit of 4,095 active databases. Releasing an entry makes capacity available again.

Each database belongs to the Pawn script that loads it. MMDB releases the script's databases automatically when the script unloads. Call `mmdb_unload()` while MMDB is operational when the script needs to release a database before unloading.

In the pinned open.mp v1.6.7.3218 runtime, ordinary script exit callbacks run before the script's databases are released. Full server shutdown can tear down MMDB before Pawn runs `OnGameModeExit` or `OnFilterScriptExit`. MMDB native availability during those callbacks is unspecified during full shutdown. Rely on automatic cleanup for databases still open at shutdown. [Script unload and server shutdown](architecture.md#script-unload-and-server-shutdown)

## Queries

| Native | Parameters | Result on success |
|---|---|---|
| `mmdb_has_record` | `name, ip, &bool:exists` | Write whether the address matches a record |
| `mmdb_has_field` | `name, ip, path, &bool:exists` | Write whether the matched record contains the path |
| `mmdb_get_string` | `name, ip, path, destination, capacity, &required` | Copy the UTF-8 string |
| `mmdb_get_int` | `name, ip, path, &value` | Write a signed 32-bit cell from an integer field |
| `mmdb_get_float` | `name, ip, path, &Float:value` | Write an MMDB float or double as a Pawn Float |
| `mmdb_get_bool` | `name, ip, path, &bool:value` | Write an MMDB boolean |
| `mmdb_get_text` | `name, ip, path, destination, capacity, &required` | Copy an integer as decimal or bytes as lowercase hex |
| `mmdb_get_type` | `name, ip, path, &MMDBType:type` | Write the format's value type |
| `mmdb_get_size` | `name, ip, path, &size` | Write the array length, map pair count, UTF-8 byte count or binary byte count |
| `mmdb_get_prefix` | `name, ip, &prefix` | Write the prefix length for the address family supplied |

Use numeric IPv4 or IPv6 without brackets, ports, CIDR suffixes or IPv6 zone identifiers. Paths are JSON Pointers up to 1,024 bytes and 32 tokens. Array indices are zero-based decimal digits with no leading zeros unless the index is `0`. Use `~0` for a tilde and `~1` for a slash inside a token.

`mmdb_get_int` accepts `int32` and unsigned integer types whose value fits a signed cell. `mmdb_get_text` preserves all integer values including `uint128`. Binary output uses two hexadecimal digits per byte, preserves zero bytes and supports up to 32,767 input bytes per call. Pawn strings cannot represent embedded NULs. `mmdb_get_string` validates UTF-8 and supports up to 1,048,575 source bytes. `mmdb_get_float` rejects non-finite values and magnitude outside the finite nonzero Pawn Float range. Double values within range round to the precision of a Pawn Float.

## Metadata and diagnostics

| Native | Parameters | Result on success |
|---|---|---|
| `mmdb_get_database_type` | `name, destination, capacity, &required` | The database type string from the file |
| `mmdb_get_build_epoch` | `name, destination, capacity, &required` | Exact unsigned decimal Unix seconds |
| `mmdb_get_ip_version` | `name, &version` | `4` or `6` |
| `mmdb_get_status_message` | `status, destination, capacity, &required` | A description of the supplied status |

## Error handling

Use unpacked Pawn strings. For every text output, `capacity` counts cells including the terminator and must be between 1 and 1,048,576. `required` must be a separate cell outside the destination. On success, `required` receives the length including the terminator. On `MMDB_BUFFER_TOO_SMALL`, only `required` changes. Other failures leave output arguments unchanged. The component checks spans against live AMX memory. Pass the actual size of the Pawn array because the VM does not expose the length of each allocation to the native.

`mmdb_is_loaded` writes false for an absent name. `mmdb_has_record` writes false when a loaded database has no matching network. `mmdb_has_field` writes false when an existing record lacks the field and returns `MMDB_NO_RECORD` for an unmatched IP. Invalid input and database errors are reported as statuses.

| Status | Meaning |
|---|---|
| `MMDB_OK` | Operation succeeded |
| `MMDB_INVALID_ARGUMENT` | Invalid native argument, name, file path or AMX memory span |
| `MMDB_NOT_LOADED` | This script has no database with that name |
| `MMDB_ALREADY_LOADED` | The name exists in this script |
| `MMDB_INVALID_IP` | The address is invalid |
| `MMDB_INVALID_PATH` | Invalid JSON Pointer syntax or array index |
| `MMDB_OPEN_FAILED` | A local regular file could not be opened or mapped |
| `MMDB_INVALID_DATABASE` | File layout or metadata failed validation |
| `MMDB_UNSUPPORTED_DATABASE` | The reader rejected the format version or record width |
| `MMDB_NO_RECORD` | The database has no matching network |
| `MMDB_NO_FIELD` | The record has no matching field |
| `MMDB_TYPE_MISMATCH` | The requested operation does not accept that value type |
| `MMDB_OUT_OF_RANGE` | The value or file exceeds the supported representation |
| `MMDB_BUFFER_TOO_SMALL` | A valid destination is too small |
| `MMDB_INVALID_UTF8` | Text contains invalid UTF-8 or an embedded NUL |
| `MMDB_RESOURCE_LIMIT` | An active database limit or a reader allocation limit was reached |
| `MMDB_DECODER_ERROR` | An error occurred while reading the record |
| `MMDB_INTERNAL_ERROR` | A C++ exception was contained at the native boundary |
| `MMDB_UNSUPPORTED_FAMILY` | An IPv6 query was made against an IPv4-only database |
