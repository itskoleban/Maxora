#include <maxora>
native print(const string[]);
forward OnFilterScriptInit();
forward OnFilterScriptExit();
main() {}
public OnFilterScriptInit()
{
    new MaxoraDb:database, MaxoraResult:result, value;
    if (Maxora_Load("scriptfiles/decoder.mmdb", database) != MAXORA_OK ||
        Maxora_Lookup(database, "1.1.1.1", result) != MAXORA_OK ||
        Maxora_GetInt(result, "/uint16", value) != MAXORA_OK || value != 100) {
        print("MAXORA_FAIL: side");
        return 0;
    }
    // Deliberately retain both resources until AMX unload.
    print("MAXORA_SIDE_PASS");
    return 1;
}
public OnFilterScriptExit() { print("MAXORA_SIDE_EXIT"); return 1; }
