#if defined _maxora_included
    #endinput
#endif
#define _maxora_included
#define MAXORA_VERSION "2.0.0"
#define MAXORA_INVALID_DB (MaxoraDb:0)
#define MAXORA_INVALID_RESULT (MaxoraResult:0)

// Every native returns a MaxoraStatus. Success is 0; never test it as a bool.
enum MaxoraStatus {
    MAXORA_OK = 0,
    MAXORA_INVALID_ARGUMENT, MAXORA_INVALID_HANDLE, MAXORA_WRONG_OWNER,
    MAXORA_INVALID_IP, MAXORA_INVALID_PATH, MAXORA_OPEN_FAILED,
    MAXORA_INVALID_DATABASE, MAXORA_UNSUPPORTED_DATABASE, MAXORA_NO_RECORD,
    MAXORA_NO_FIELD, MAXORA_TYPE_MISMATCH, MAXORA_OUT_OF_RANGE,
    MAXORA_BUFFER_TOO_SMALL, MAXORA_INVALID_UTF8, MAXORA_RESOURCE_LIMIT,
    MAXORA_DECODER_ERROR, MAXORA_INTERNAL_ERROR, MAXORA_UNSUPPORTED_FAMILY
};
enum MaxoraType {
    MAXORA_TYPE_STRING = 2, MAXORA_TYPE_DOUBLE = 3, MAXORA_TYPE_BYTES = 4,
    MAXORA_TYPE_UINT16 = 5, MAXORA_TYPE_UINT32 = 6, MAXORA_TYPE_MAP = 7,
    MAXORA_TYPE_INT32 = 8, MAXORA_TYPE_UINT64 = 9, MAXORA_TYPE_UINT128 = 10,
    MAXORA_TYPE_ARRAY = 11, MAXORA_TYPE_BOOLEAN = 14, MAXORA_TYPE_FLOAT = 15
};

/** Resources belong to the calling script. Unload automatically frees all its
 * databases and results. A result pins a database snapshot through reload/unload.
 * Calls are synchronous on the server thread. Packed Pawn strings are rejected.
 * All strings are unpacked UTF-8 bytes. No native performs DNS or downloads.
 * Unless specified, outputs are unchanged on failure. Paths use JSON Pointer:
 * "" is root; "/country/iso_code"; ~0 escapes ~; ~1 escapes /. */

/** Open local path (1..4095 bytes); set database only on success. */
native MaxoraStatus:Maxora_Load(const path[], &MaxoraDb:database);
/** Open replacement first; failed reload preserves existing database/results. */
native MaxoraStatus:Maxora_Reload(MaxoraDb:database, const path[]);
/** Invalidate database handle. Existing result snapshots remain readable. */
native MaxoraStatus:Maxora_Unload(MaxoraDb:database);
/** Set valid to whether database identifies a live database owned by this script. */
native MaxoraStatus:Maxora_IsValid(MaxoraDb:database, &bool:valid);
/** Strict numeric IPv4/IPv6; NO_RECORD differs from INVALID_IP. Release result. */
native MaxoraStatus:Maxora_Lookup(MaxoraDb:database, const ip[], &MaxoraResult:result);
/** Invalidate result and release its snapshot reference. */
native MaxoraStatus:Maxora_Release(MaxoraResult:result);
/** String extraction. capacity includes terminator; required includes terminator.
 * BUFFER_TOO_SMALL sets required and leaves destination unchanged. No truncation.
 * Maximum output capacity is 1048576 cells. */
native MaxoraStatus:Maxora_GetString(MaxoraResult:result, const path[], destination[], capacity, &required);
/** Signed 32-bit integer; larger numeric values produce OUT_OF_RANGE. */
native MaxoraStatus:Maxora_GetInt(MaxoraResult:result, const path[], &value);
/** MMDB float/double to finite Pawn Float; unrepresentable results fail. */
native MaxoraStatus:Maxora_GetFloat(MaxoraResult:result, const path[], &Float:value);
/** Extract an MMDB boolean; does not coerce integers/strings. */
native MaxoraStatus:Maxora_GetBoolean(MaxoraResult:result, const path[], &bool:value);
/** Integers as exact decimal text, bytes as lowercase hex. Same output contract. */
native MaxoraStatus:Maxora_GetText(MaxoraResult:result, const path[], destination[], capacity, &required);
/** Inspect the MMDB type; maps and arrays are inspectable. */
native MaxoraStatus:Maxora_GetType(MaxoraResult:result, const path[], &MaxoraType:type);
/** Map pairs / array elements / string bytes / binary bytes; otherwise TYPE_MISMATCH. */
native MaxoraStatus:Maxora_GetSize(MaxoraResult:result, const path[], &size);
/** Missing field succeeds with exists=false; malformed paths/decoder errors fail. */
native MaxoraStatus:Maxora_HasField(MaxoraResult:result, const path[], &bool:exists);
/** Prefix length normalized to the requested address family. */
native MaxoraStatus:Maxora_GetPrefix(MaxoraResult:result, &prefix);
/** Database edition/type string. Same required/capacity contract as GetString. */
native MaxoraStatus:Maxora_GetDatabaseType(MaxoraDb:database, destination[], capacity, &required);
/** Metadata build epoch as exact unsigned decimal text. */
native MaxoraStatus:Maxora_GetBuildEpoch(MaxoraDb:database, destination[], capacity, &required);
/** Metadata IP version (4 or 6). IPv6 databases can also contain IPv4 records. */
native MaxoraStatus:Maxora_GetIpVersion(MaxoraDb:database, &version);
/** Pure status explanation; no shared or per-thread last-error state. */
native MaxoraStatus:Maxora_GetStatusMessage(MaxoraStatus:status, destination[], capacity, &required);

/** Optional edition-specific helpers return NO_FIELD if absent. */
stock MaxoraStatus:Maxora_GetCountryCode(MaxoraResult:result, destination[], capacity, &required)
    return Maxora_GetString(result, "/country/iso_code", destination, capacity, required);
stock MaxoraStatus:Maxora_GetCityName(MaxoraResult:result, destination[], capacity, &required)
    return Maxora_GetString(result, "/city/names/en", destination, capacity, required);
stock MaxoraStatus:Maxora_GetSubdivisionCode(MaxoraResult:result, destination[], capacity, &required)
    return Maxora_GetString(result, "/subdivisions/0/iso_code", destination, capacity, required);
stock MaxoraStatus:Maxora_GetASN(MaxoraResult:result, &value)
    return Maxora_GetInt(result, "/autonomous_system_number", value);
stock MaxoraStatus:Maxora_GetOrganization(MaxoraResult:result, destination[], capacity, &required)
    return Maxora_GetString(result, "/autonomous_system_organization", destination, capacity, required);
stock MaxoraStatus:Maxora_GetLatitude(MaxoraResult:result, &Float:value)
    return Maxora_GetFloat(result, "/location/latitude", value);
stock MaxoraStatus:Maxora_GetLongitude(MaxoraResult:result, &Float:value)
    return Maxora_GetFloat(result, "/location/longitude", value);
