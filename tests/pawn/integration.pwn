#include <omp_mmdb>
#pragma rational Float

native print(const string[]);
native printf(const format[], {Float,_}:...);
native SetTimer(const function[], interval, bool:repeating);
native SetModeRestartTime(Float:seconds);
native SendRconCommand(const command[]);
native SetSVarInt(const name[], value);
native GetSVarInt(const name[]);
native strcmp(const first[], const second[], bool:ignorecase, length);

#if !defined MMDB_TEST_DURATION_MS
	#define MMDB_TEST_DURATION_MS 10000
#endif

#define DECODER "scriptfiles/decoder.mmdb"
#define IPV4 "scriptfiles/ipv4.mmdb"
#define IPV6 "scriptfiles/ipv6.mmdb"
#define MISSING "scriptfiles/missing.mmdb"
#define TRUNCATED "scriptfiles/truncated.mmdb"

forward OnGameModeInit();
forward OnGameModeExit();
forward MMDBTestTick();
forward MMDBTestFinish();

new gTicks;
main() {}

stock Check(bool:condition, const description[])
{
	if (!condition)
	{
		printf("MMDB_FAIL: %s", description);
		return 0;
	}
	return 1;
}

public OnGameModeInit()
{
	SetModeRestartTime(0.1);
	new bool:flag;
	Check(mmdb_is_loaded("shared", flag) == MMDB_OK && !flag, "fresh gamemode namespace");
	if (GetSVarInt("mmdb_gmx_done") == 1)
		print("MMDB_MAIN_CLEANUP_PASS");
	if (!Check(mmdb_load("shared", DECODER) == MMDB_OK, "load")) return 0;
	Check(mmdb_load("shared", DECODER) == MMDB_ALREADY_LOADED, "duplicate name");
	Check(mmdb_load("bad/name", DECODER) == MMDB_INVALID_ARGUMENT, "invalid name");

	new value, required, text[128], Float:number, MMDBType:type;
	Check(mmdb_has_record("shared", "1.1.1.1", flag) == MMDB_OK && flag, "has record");
	Check(mmdb_get_int("shared", "1.1.1.1", "/uint16", value) == MMDB_OK && value == 100, "integer");
	Check(mmdb_get_string("shared", "1.1.1.1", "/utf8_string", text, sizeof text, required) == MMDB_OK, "UTF-8 string");
	text[0] = 'X';
	Check(mmdb_get_string("shared", "1.1.1.1", "/utf8_string", text, 1, required) == MMDB_BUFFER_TOO_SMALL && required > 1 && text[0] == 'X', "no partial write");
	Check(mmdb_get_text("shared", "1.1.1.1", "/uint128", text, sizeof text, required) == MMDB_OK, "uint128 text");
	Check(mmdb_get_text("shared", "1.1.1.1", "/bytes", text, sizeof text, required) == MMDB_OK, "bytes as hex");
	Check(mmdb_get_bool("shared", "1.1.1.1", "/boolean", flag) == MMDB_OK && flag, "boolean");
	Check(mmdb_get_float("shared", "1.1.1.1", "/double", number) == MMDB_OK, "float");
	Check(mmdb_get_type("shared", "1.1.1.1", "/array", type) == MMDB_OK && type == MMDB_TYPE_ARRAY, "array type");
	Check(mmdb_get_type("shared", "1.1.1.1", "/map", type) == MMDB_OK && type == MMDB_TYPE_MAP, "map type");
	Check(mmdb_get_size("shared", "1.1.1.1", "/array", value) == MMDB_OK && value == 3, "array size");
	Check(mmdb_get_size("shared", "1.1.1.1", "/map", value) == MMDB_OK && value > 0, "map size");
	Check(mmdb_load("nested", "scriptfiles/nested.mmdb") == MMDB_OK, "nested map load");
	Check(mmdb_get_int("nested", "1.1.1.1", "/map1/map2/array/0/map3/c", value) == MMDB_OK && value == 3, "nested map and array");
	Check(mmdb_unload("nested") == MMDB_OK, "nested map unload");
	Check(mmdb_has_field("shared", "1.1.1.1", "/missing", flag) == MMDB_OK && !flag, "missing field");
	Check(mmdb_has_field("shared", "1.1.1.1", "/bad~2escape", flag) == MMDB_INVALID_PATH, "invalid escape");
	Check(mmdb_get_prefix("shared", "1.1.1.1", value) == MMDB_OK, "prefix");
	Check(mmdb_get_database_type("shared", text, sizeof text, required) == MMDB_OK, "metadata type");
	Check(mmdb_get_build_epoch("shared", text, sizeof text, required) == MMDB_OK, "metadata epoch");
	Check(mmdb_get_ip_version("shared", value) == MMDB_OK && value == 6, "metadata IP version");
	Check(mmdb_get_status_message(MMDB_OK, text, sizeof text, required) == MMDB_OK, "status message");
	Check(mmdb_has_record("shared", "localhost", flag) == MMDB_INVALID_IP, "numeric IP only");
	Check(mmdb_reload("shared", MISSING) == MMDB_OPEN_FAILED, "missing replacement");
	Check(mmdb_reload("shared", TRUNCATED) != MMDB_OK, "truncated replacement");
	Check(mmdb_get_int("shared", "1.1.1.1", "/uint16", value) == MMDB_OK && value == 100, "original after failed reload");
	Check(mmdb_reload("shared", IPV4) == MMDB_OK, "replace with different database");
	Check(mmdb_get_ip_version("shared", value) == MMDB_OK && value == 4, "metadata after replacement");
	Check(mmdb_reload("shared", DECODER) == MMDB_OK, "restore decoder");
	Check(mmdb_load("v4", IPV4) == MMDB_OK, "IPv4 load");
	Check(mmdb_load("v6", IPV6) == MMDB_OK, "IPv6 load");
#if defined MMDB_TEST_DBIP_CITY
	Check(mmdb_load("dbip", "scriptfiles/dbip-city.mmdb") == MMDB_OK, "DB-IP City Lite load");
	Check(mmdb_get_string("dbip", "8.8.8.8", "/country/iso_code", text, sizeof text, required) == MMDB_OK &&
		strcmp(text, "US", false, sizeof text) == 0, "DB-IP City Lite nested country code");
	Check(mmdb_unload("dbip") == MMDB_OK, "DB-IP City Lite unload");
	print("MMDB_DBIP_PASS");
#endif
#if defined MMDB_TEST_IPINFO_LITE
	Check(mmdb_load("ipinfo", "scriptfiles/ipinfo-lite.mmdb") == MMDB_OK, "IPinfo Lite load");
	Check(mmdb_get_string("ipinfo", "1.0.0.1", "/country_code", text, sizeof text, required) == MMDB_OK &&
		strcmp(text, "AU", false, sizeof text) == 0, "IPinfo Lite flat country code");
	Check(mmdb_get_string("ipinfo", "1.0.0.1", "/asn", text, sizeof text, required) == MMDB_OK &&
		strcmp(text, "AS13335", false, sizeof text) == 0, "IPinfo Lite ASN string");
	Check(mmdb_get_int("ipinfo", "1.0.0.1", "/asn", value) == MMDB_TYPE_MISMATCH, "IPinfo Lite strict ASN type");
	Check(mmdb_unload("ipinfo") == MMDB_OK, "IPinfo Lite unload");
	print("MMDB_IPINFO_PASS");
#endif
	print("MMDB_MAIN_PASS");
	SetTimer("MMDBTestTick", 100, true);
	return 1;
}

public MMDBTestTick()
{
	new value, bool:exists;
	for (new i = 0; i < 100; ++i)
	{
		if (!Check(mmdb_get_int("shared", "1.1.1.1", "/array/2", value) == MMDB_OK && value == 3, "tick direct query"))
			return 0;
	}
	Check(mmdb_has_field("shared", "1.1.1.1", "/missing", exists) == MMDB_OK && !exists, "tick missing field");
	Check(mmdb_get_prefix("v6", "::2:0:1a", value) == MMDB_OK && value == 122, "tick IPv6 prefix");
	Check(mmdb_has_record("v4", "2.3.4.5", exists) == MMDB_OK && !exists, "tick missing record");
	if (gTicks % 10 == 0)
	{
		Check(mmdb_reload("shared", DECODER) == MMDB_OK, "tick reload");
		Check(mmdb_reload("shared", MISSING) == MMDB_OPEN_FAILED, "tick failed reload");
		Check(mmdb_unload("v4") == MMDB_OK, "tick unload");
		Check(mmdb_is_loaded("v4", exists) == MMDB_OK && !exists, "tick unloaded name");
		Check(mmdb_load("v4", IPV4) == MMDB_OK, "tick reuse name");
		Check(mmdb_get_prefix("v4", "1.1.1.3", value) == MMDB_OK && value == 31, "tick IPv4 prefix");
	}
	if (gTicks++ == 0)
	{
		print("MMDB_AFTER_RESET_PASS");
		if (GetSVarInt("mmdb_gmx_done") == 1)
			SetTimer("MMDBTestFinish", MMDB_TEST_DURATION_MS + 500, false);
	}
	if (GetSVarInt("mmdb_gmx_done") == 0)
	{
		if (gTicks == 2) SendRconCommand("reloadfs mmdb_side");
		if (gTicks == 4) SendRconCommand("unloadfs mmdb_side");
		if (gTicks == 6) SendRconCommand("loadfs mmdb_side");
		if (gTicks == 8)
		{
			SetSVarInt("mmdb_gmx_done", 1);
			SendRconCommand("gmx");
		}
	}
	if (gTicks % 10 == 0) print("MMDB_SOAK_TICK");
	return 1;
}

public OnGameModeExit()
{
	// Leave all three databases open to exercise automatic owner cleanup.
	return 1;
}

public MMDBTestFinish()
{
	print("MMDB_WORKLOAD_FINISHED");
	SendRconCommand("exit");
	return 1;
}
