#include <maxora>
native print(const string[]);
native SetSVarInt(const name[], value);
native GetSVarInt(const name[]);
forward OnFilterScriptInit();
forward OnFilterScriptExit();
main() {}
public OnFilterScriptInit()
{
    new MaxoraDb:old = MaxoraDb:GetSVarInt("maxora_side_old_database");
    if (old != MAXORA_INVALID_DB && Maxora_Unload(old) != MAXORA_INVALID_HANDLE) {
        print("MAXORA_FAIL: filterscript resource survived unload");
        return 0;
    }
    if (old != MAXORA_INVALID_DB) print("MAXORA_SIDE_CLEANUP_PASS");
    new MaxoraDb:database, MaxoraResult:result, value;
    if (Maxora_Load("scriptfiles/decoder.mmdb", database) != MAXORA_OK ||
        Maxora_Lookup(database, "1.1.1.1", result) != MAXORA_OK ||
        Maxora_GetInt(result, "/uint16", value) != MAXORA_OK || value != 100) {
        print("MAXORA_FAIL: side");
        return 0;
    }
    // Deliberately retain both resources until AMX unload.
    SetSVarInt("maxora_side_old_database", _:database);
    print("MAXORA_SIDE_PASS");
    return 1;
}
public OnFilterScriptExit() { print("MAXORA_SIDE_EXIT"); return 1; }
