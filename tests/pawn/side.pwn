#include <omp_mmdb>

native print(const string[]);
native SetSVarInt(const name[], value);
native GetSVarInt(const name[]);

forward OnFilterScriptInit();
forward OnFilterScriptExit();

main() {}

public OnFilterScriptInit()
{
	new bool:exists, prefix;
	if (mmdb_is_loaded("shared", exists) != MMDB_OK || exists ||
		mmdb_load("shared", "scriptfiles/ipv4.mmdb") != MMDB_OK ||
		mmdb_get_prefix("shared", "1.1.1.3", prefix) != MMDB_OK || prefix != 31)
	{
		print("MMDB_FAIL: filterscript namespace or IPv4 lookup");
		return 0;
	}
	if (GetSVarInt("mmdb_side_seen") != 0)
		print("MMDB_SIDE_CLEANUP_PASS");
	SetSVarInt("mmdb_side_seen", 1);
	print("MMDB_SIDE_PASS");
	// The gamemode uses the same name for a different database.
	return 1;
}

public OnFilterScriptExit()
{
	print("MMDB_SIDE_EXIT");
	return 1;
}
