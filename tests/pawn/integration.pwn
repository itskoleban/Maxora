#include <maxora>
#pragma rational Float
native print(const string[]);
native printf(const format[], {Float,_}:...);
native SetTimer(const function[], interval, bool:repeating);
native SetModeRestartTime(Float:seconds);
native SendRconCommand(const command[]);
native SetSVarInt(const name[], value);
native GetSVarInt(const name[]);
#if !defined MAXORA_TEST_DURATION_MS
    #define MAXORA_TEST_DURATION_MS 10000
#endif
forward OnGameModeInit();
forward OnGameModeExit();
forward MaxoraTick();
forward MaxoraFinish();
new MaxoraDb:gDatabase;
new gTicks;
main() {}
stock Check(bool:condition, const description[])
{
    if (!condition) { printf("MAXORA_FAIL: %s", description); return 0; }
    return 1;
}
public OnGameModeInit()
{
    SetModeRestartTime(0.1);
    new MaxoraDb:old = MaxoraDb:GetSVarInt("maxora_main_old_database");
    if (!Check(old == MAXORA_INVALID_DB || Maxora_Unload(old) == MAXORA_INVALID_HANDLE, "gamemode resource survived unload")) return 0;
    if (old != MAXORA_INVALID_DB) print("MAXORA_MAIN_CLEANUP_PASS");
    if (!Check(Maxora_Load("scriptfiles/decoder.mmdb", gDatabase) == MAXORA_OK, "load")) return 0;
    SetSVarInt("maxora_main_old_database", _:gDatabase);
    new MaxoraResult:result, value, required, text[128], bool:flag, Float:number, MaxoraType:type;
    if (!Check(Maxora_Lookup(gDatabase, "1.1.1.1", result) == MAXORA_OK, "lookup")) return 0;
    Check(Maxora_GetInt(result, "/uint16", value) == MAXORA_OK && value == 100, "integer");
    Check(Maxora_GetString(result, "/utf8_string", text, sizeof text, required) == MAXORA_OK, "utf8 string");
    Check(Maxora_GetString(result, "/utf8_string", text, 1, required) == MAXORA_BUFFER_TOO_SMALL && required > 1, "buffer");
    Check(Maxora_GetText(result, "/uint128", text, sizeof text, required) == MAXORA_OK, "uint128 text");
    Check(Maxora_GetBoolean(result, "/boolean", flag) == MAXORA_OK && flag, "boolean");
    Check(Maxora_GetFloat(result, "/double", number) == MAXORA_OK, "float");
    Check(Maxora_GetType(result, "/array", type) == MAXORA_OK && type == MAXORA_TYPE_ARRAY, "type");
    Check(Maxora_GetSize(result, "/array", value) == MAXORA_OK && value == 3, "size");
    Check(Maxora_HasField(result, "/missing", flag) == MAXORA_OK && !flag, "has missing");
    Check(Maxora_GetPrefix(result, value) == MAXORA_OK, "prefix");
    Check(Maxora_GetDatabaseType(gDatabase, text, sizeof text, required) == MAXORA_OK, "metadata type");
    Check(Maxora_GetBuildEpoch(gDatabase, text, sizeof text, required) == MAXORA_OK, "metadata epoch");
    Check(Maxora_GetIpVersion(gDatabase, value) == MAXORA_OK, "metadata ip version");
    Check(Maxora_GetStatusMessage(MAXORA_OK, text, sizeof text, required) == MAXORA_OK, "status message");
    Check(Maxora_Reload(gDatabase, "scriptfiles/missing.mmdb") == MAXORA_OPEN_FAILED, "failed reload");
    Check(Maxora_Reload(gDatabase, "scriptfiles/decoder.mmdb") == MAXORA_OK, "reload");
    Check(Maxora_GetInt(result, "/uint16", value) == MAXORA_OK, "snapshot");
    Check(Maxora_Release(result) == MAXORA_OK, "release");
    Check(Maxora_Release(result) == MAXORA_INVALID_HANDLE, "stale result");
    Check(Maxora_IsValid(gDatabase, flag) == MAXORA_OK && flag, "valid");
    Check(Maxora_Lookup(gDatabase, "localhost", result) == MAXORA_INVALID_IP, "numeric ip only");
    new MaxoraDb:v6;
    Check(Maxora_Load("scriptfiles/ipv6.mmdb", v6) == MAXORA_OK, "v6 load");
    Check(Maxora_Lookup(v6, "::2:0:1a", result) == MAXORA_OK, "v6 lookup");
    Check(Maxora_Unload(v6) == MAXORA_OK, "unload");
    Check(Maxora_GetPrefix(result, value) == MAXORA_OK, "snapshot after unload");
    Check(Maxora_Release(result) == MAXORA_OK, "v6 release");
    print("MAXORA_MAIN_PASS");
    SetTimer("MaxoraTick", 100, true);
    return 1;
}
public MaxoraTick()
{
    for (new i = 0; i < 30; i++) {
        new MaxoraResult:result, value;
        if (!Check(Maxora_Lookup(gDatabase, "1.1.1.1", result) == MAXORA_OK, "tick lookup")) return 0;
        Check(Maxora_GetInt(result, "/array/2", value) == MAXORA_OK && value == 3, "tick value");
        Check(Maxora_Release(result) == MAXORA_OK, "tick release");
    }
    if (gTicks++ == 0) {
        print("MAXORA_AFTER_RESET_PASS");
        if (GetSVarInt("maxora_gmx_done") == 1)
            SetTimer("MaxoraFinish", MAXORA_TEST_DURATION_MS + 500, false);
    }
    if (GetSVarInt("maxora_gmx_done") == 0) {
        if (gTicks == 2) SendRconCommand("reloadfs maxora_side");
        if (gTicks == 4) SendRconCommand("unloadfs maxora_side");
        if (gTicks == 6) SendRconCommand("loadfs maxora_side");
        if (gTicks == 8) {
            SetSVarInt("maxora_gmx_done", 1);
            SendRconCommand("gmx");
        }
    }
    if (gTicks % 10 == 0) print("MAXORA_SOAK_TICK");
    return 1;
}
public OnGameModeExit()
{
    // Deliberately rely on per-script automatic cleanup.
    return 1;
}
public MaxoraFinish()
{
    print("MAXORA_WORKLOAD_FINISHED");
    SendRconCommand("exit");
    return 1;
}
