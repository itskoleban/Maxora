#if defined _omp_mmdb_included
	#endinput
#endif
#define _omp_mmdb_included
#define MMDB_VERSION "0.1.0"

enum MMDBStatus {
	MMDB_OK = 0,
	MMDB_INVALID_ARGUMENT, MMDB_NOT_LOADED, MMDB_ALREADY_LOADED,
	MMDB_INVALID_IP, MMDB_INVALID_PATH, MMDB_OPEN_FAILED,
	MMDB_INVALID_DATABASE, MMDB_UNSUPPORTED_DATABASE, MMDB_NO_RECORD,
	MMDB_NO_FIELD, MMDB_TYPE_MISMATCH, MMDB_OUT_OF_RANGE,
	MMDB_BUFFER_TOO_SMALL, MMDB_INVALID_UTF8, MMDB_RESOURCE_LIMIT,
	MMDB_DECODER_ERROR, MMDB_INTERNAL_ERROR, MMDB_UNSUPPORTED_FAMILY
};

enum MMDBType {
	MMDB_TYPE_STRING = 2, MMDB_TYPE_DOUBLE = 3, MMDB_TYPE_BYTES = 4,
	MMDB_TYPE_UINT16 = 5, MMDB_TYPE_UINT32 = 6, MMDB_TYPE_MAP = 7,
	MMDB_TYPE_INT32 = 8, MMDB_TYPE_UINT64 = 9, MMDB_TYPE_UINT128 = 10,
	MMDB_TYPE_ARRAY = 11, MMDB_TYPE_BOOLEAN = 14, MMDB_TYPE_FLOAT = 15
};

// Names are case-sensitive and local to the calling script. Each contains
// 1–63 ASCII letters, digits, underscores, hyphens or dots. The server
// releases a script's databases automatically when it unloads.
native MMDBStatus:mmdb_load(const database[], const file[]);
native MMDBStatus:mmdb_reload(const database[], const file[]);
native MMDBStatus:mmdb_unload(const database[]);
native MMDBStatus:mmdb_is_loaded(const database[], &bool:loaded);

// Queries accept numeric IPv4 and IPv6 and use JSON Pointer paths.
// "" selects the record root, "~0" escapes "~", and "~1" escapes "/".
// Outputs are unchanged on failure unless documented below.
native MMDBStatus:mmdb_has_record(const database[], const ip[], &bool:exists);

// Text capacity and required length include the terminating cell.
// MMDB_BUFFER_TOO_SMALL sets required and leaves destination untouched.
// Use unpacked Pawn strings with one UTF-8 byte per cell.
native MMDBStatus:mmdb_get_string(const database[], const ip[], const path[], destination[], capacity, &required);
native MMDBStatus:mmdb_get_int(const database[], const ip[], const path[], &value);
native MMDBStatus:mmdb_get_float(const database[], const ip[], const path[], &Float:value);
native MMDBStatus:mmdb_get_bool(const database[], const ip[], const path[], &bool:value);
// Returns an exact decimal integer or lowercase hexadecimal bytes.
native MMDBStatus:mmdb_get_text(const database[], const ip[], const path[], destination[], capacity, &required);
native MMDBStatus:mmdb_get_type(const database[], const ip[], const path[], &MMDBType:type);
native MMDBStatus:mmdb_get_size(const database[], const ip[], const path[], &size);
native MMDBStatus:mmdb_has_field(const database[], const ip[], const path[], &bool:exists);
native MMDBStatus:mmdb_get_prefix(const database[], const ip[], &prefix);

native MMDBStatus:mmdb_get_database_type(const database[], destination[], capacity, &required);
// Returns exact unsigned decimal Unix time in seconds.
native MMDBStatus:mmdb_get_build_epoch(const database[], destination[], capacity, &required);
native MMDBStatus:mmdb_get_ip_version(const database[], &version);

native MMDBStatus:mmdb_get_status_message(MMDBStatus:status, destination[], capacity, &required);
