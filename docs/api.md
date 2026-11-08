# Pawn API reference

Include `maxora.inc` with `#include <maxora>`. All natives return the tagged `MaxoraStatus`; **MAXORA_OK is 0**. Database and lookup handles use `MaxoraDb` and `MaxoraResult` tags, with zero invalid. No 1.x natives are provided. All resource access is owned by the calling script and synchronous on the Pawn execution thread.

## Resource operations

| Native | Behavior |
| --- | --- |
| `Maxora_Load(const path[], &MaxoraDb:database)` | Opens a regular local MMDB file and writes a new database handle on success. Public Pawn path limit: 4,095 UTF-8 bytes. |
| `Maxora_Reload(MaxoraDb:database, const path[])` | Opens and validates replacement before swapping; failure preserves existing mapping. Existing results retain their old snapshots. |
| `Maxora_Unload(MaxoraDb:database)` | Invalidates database handle; outstanding results remain readable. |
| `Maxora_IsValid(MaxoraDb:database, &bool:valid)` | Returns OK and sets false for an expired/invalid/wrong-kind handle. A live other-script handle returns WRONG_OWNER without writing output. |
| `Maxora_Lookup(MaxoraDb:database, const ip[], &MaxoraResult:result)` | Strict numeric IPv4/IPv6 lookup; writes a reusable result handle. NO_RECORD means no matching entry. Release it after use. |
| `Maxora_Release(MaxoraResult:result)` | Invalidates result and releases snapshot reference. |

Script unload releases its resources automatically. Filterscripts cannot access another script's handles. Reset preserves active-script resources because of open.mp's gamemode initialization ordering. Component shutdown releases everything. Do not persist handles across process restarts.

## Field operations

| Native | Accepted type and output |
| --- | --- |
| `Maxora_GetString(MaxoraResult:result, const path[], destination[], capacity, &required)` | Valid UTF-8 without embedded NUL, copied as unpacked byte cells. |
| `Maxora_GetInt(MaxoraResult:result, const path[], &value)` | int32 or unsigned integer that fits signed 32-bit Pawn cell; otherwise OUT_OF_RANGE. |
| `Maxora_GetFloat(MaxoraResult:result, const path[], &Float:value)` | MMDB float or double; finite binary32 conversion, with precision rounding; nonfinite/overflow/underflow rejected. |
| `Maxora_GetBoolean(MaxoraResult:result, const path[], &bool:value)` | Boolean only; no truthiness coercion. |
| `Maxora_GetText(MaxoraResult:result, const path[], destination[], capacity, &required)` | Exact decimal for uint16/32/64/128 and int32; lowercase hexadecimal for bytes, limited to 32,767 source bytes. |
| `Maxora_GetType(MaxoraResult:result, const path[], &MaxoraType:type)` | Logical MMDB type; map and array supported. |
| `Maxora_GetSize(MaxoraResult:result, const path[], &size)` | Map pair count, array element count, string byte count or bytes length. Scalar types fail TYPE_MISMATCH. |
| `Maxora_HasField(MaxoraResult:result, const path[], &bool:exists)` | Missing field returns OK and false. Malformed paths and decoder errors propagate. |
| `Maxora_GetPrefix(MaxoraResult:result, &prefix)` | Address-family prefix length (0..32 IPv4, 0..128 IPv6). |

Paths use JSON Pointer: `""` is the record root, `/country/iso_code` selects map keys, `/subdivisions/0/iso_code` selects an array entry. `~0` encodes a literal tilde and `~1` a literal slash. Empty keys are supported. Paths are limited to 1,024 UTF-8 bytes and 32 tokens. libmaxminddb array semantics apply, including negative indices counted from the end. Map keys need not follow any GeoLite schema. This API does not enumerate unknown map keys or serialize whole containers.

Type values: STRING=2, DOUBLE=3, BYTES=4, UINT16=5, UINT32=6, MAP=7, INT32=8, UINT64=9, UINT128=10, ARRAY=11, BOOLEAN=14, FLOAT=15, exposed as `MAXORA_TYPE_*`.

## Metadata and diagnostics

| Native | Output |
| --- | --- |
| `Maxora_GetDatabaseType(MaxoraDb:database, destination[], capacity, &required)` | Database edition/type metadata. |
| `Maxora_GetBuildEpoch(MaxoraDb:database, destination[], capacity, &required)` | Exact unsigned decimal timestamp, avoiding signed-cell timestamp overflow. |
| `Maxora_GetIpVersion(MaxoraDb:database, &version)` | Metadata IP version 4 or 6. |
| `Maxora_GetStatusMessage(MaxoraStatus:status, destination[], capacity, &required)` | Pure English description of supplied status; invalid enum values fail INVALID_ARGUMENT. No shared last-error state. |

## String output and boundary contract

Capacity is the true destination array length **in cells**, including terminator. Its allowed range is 1..1,048,576. Required includes the terminator. On OK the complete value and required length are written. On BUFFER_TOO_SMALL only required is written; destination remains unchanged. No native truncates with success. Required must not overlap destination. Other failures leave outputs unchanged. Input/output logical arrays and output references should be distinct.

Packed Pawn strings are rejected. Strings must be unpacked cells containing UTF-8 byte values 0..255 and a NUL terminator. Native argument counts, address alignment, allocated AMX ranges, references, bounded reads, and writable spans are checked. AMX does not expose source-level array bounds: passing a false capacity can still overwrite an adjacent script variable within allocated AMX memory. Always use `sizeof destination`.

## Status codes

| Constant | Value | Meaning |
| --- | --- | --- |
| MAXORA_OK | 0 | Operation completed. |
| MAXORA_INVALID_ARGUMENT | 1 | Argument count, reference, range, capacity, string representation or file path invalid. |
| MAXORA_INVALID_HANDLE | 2 | Expired, invalid or wrong-kind resource handle. |
| MAXORA_WRONG_OWNER | 3 | Live resource belongs to another script. |
| MAXORA_INVALID_IP | 4 | Not a strict numeric IPv4/IPv6 literal. |
| MAXORA_INVALID_PATH | 5 | Malformed or excessive field path. |
| MAXORA_OPEN_FAILED | 6 | Missing, unreadable or nonregular file. |
| MAXORA_INVALID_DATABASE | 7 | Invalid structure or metadata, empty or oversized file. |
| MAXORA_UNSUPPORTED_DATABASE | 8 | libmaxminddb does not recognize the format. |
| MAXORA_NO_RECORD | 9 | Address has no matching record. |
| MAXORA_NO_FIELD | 10 | Path does not match data. |
| MAXORA_TYPE_MISMATCH | 11 | Requested conversion does not accept this logical type. |
| MAXORA_OUT_OF_RANGE | 12 | Conversion exceeds documented range/representation. |
| MAXORA_BUFFER_TOO_SMALL | 13 | Complete output does not fit; required is available. |
| MAXORA_INVALID_UTF8 | 14 | Invalid UTF-8 or embedded NUL in a database string. |
| MAXORA_RESOURCE_LIMIT | 15 | Owner/global quota or handle-generation domain exhausted. |
| MAXORA_DECODER_ERROR | 16 | Decoder detected invalid database data/tree. |
| MAXORA_INTERNAL_ERROR | 17 | Unexpected C++ failure, caught at native boundary. |
| MAXORA_UNSUPPORTED_FAMILY | 18 | IPv6 queried against IPv4-only database. |

Input checks precede operations where required for memory safety; do not depend on which error wins when several inputs are invalid. Successful calls do not erase any last-error record because none exists.

## Edition-specific stock helpers

`Maxora_GetCountryCode`, `Maxora_GetCityName`, `Maxora_GetSubdivisionCode` and `Maxora_GetOrganization` take `(result, destination, capacity, &required)` and delegate to GetString. City name uses English. `Maxora_GetASN(result, &value)` delegates to GetInt and may reject an ASN above signed-cell range; use GetText on `/autonomous_system_number` for its exact decimal representation. `Maxora_GetLatitude` and `Maxora_GetLongitude` take `(result, &Float:value)` and delegate to GetFloat. No helper guarantees that its field exists in any particular record. Use generic paths for alternate languages or database editions.

The declarations and helper bodies are also documented in [maxora.inc](../include/maxora.inc). [geolocation.pwn](../examples/geolocation.pwn) demonstrates load, query, checked output and cleanup.

String extraction is bounded to 1,048,575 database bytes before UTF-8 validation; larger strings return OUT_OF_RANGE. GetSize can inspect their length without copying. This caps selected string validation/copy work independently of the database file size.
