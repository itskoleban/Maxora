#include <maxora>

native print(const string[]);
native GetPlayerIp(playerid, ip[], len);
forward OnGameModeInit();
forward OnPlayerConnect(playerid);
forward OnGameModeExit();
new MaxoraDb:gDatabase = MAXORA_INVALID_DB;

main() {}
public OnGameModeInit()
{
    new MaxoraStatus:status = Maxora_Load("scriptfiles/GeoLite2-City.mmdb", gDatabase);
    if (status != MAXORA_OK) {
        new message[128], required;
        if (Maxora_GetStatusMessage(status, message, sizeof message, required) == MAXORA_OK)
            print(message);
    }
    return 1;
}
public OnPlayerConnect(playerid)
{
    new ip[46], MaxoraResult:result = MAXORA_INVALID_RESULT;
    GetPlayerIp(playerid, ip, sizeof ip);
    if (Maxora_Lookup(gDatabase, ip, result) == MAXORA_OK) {
        new country[3], required;
        if (Maxora_GetCountryCode(result, country, sizeof country, required) == MAXORA_OK)
            print(country);
        Maxora_Release(result);
    }
    return 1;
}
public OnGameModeExit()
{
    if (gDatabase != MAXORA_INVALID_DB) Maxora_Unload(gDatabase);
    gDatabase = MAXORA_INVALID_DB;
    return 1;
}
