#include <open.mp>
#include <omp_mmdb>

main() {}

stock PrintMMDBError(const operation[], MMDBStatus:status)
{
	new message[128], required;
	if (mmdb_get_status_message(status, message, sizeof(message), required) == MMDB_OK)
		printf("[MMDB] %s: %s", operation, message);
	else
		printf("[MMDB] %s: error %d", operation, _:status);
}

public OnGameModeInit()
{
	new MMDBStatus:status = mmdb_load("city", "scriptfiles/GeoLite2-City.mmdb");
	if (status != MMDB_OK)
		PrintMMDBError("load city", status);

	status = mmdb_load("asn", "scriptfiles/GeoLite2-ASN.mmdb");
	if (status != MMDB_OK)
		PrintMMDBError("load asn", status);
	return 1;
}

public OnPlayerConnect(playerid)
{
	new ip[46], country[128], asn[40], required;
	GetPlayerIp(playerid, ip, sizeof(ip));

	new MMDBStatus:status = mmdb_get_string("city", ip, "/country/names/en", country, sizeof(country), required);
	if (status == MMDB_OK)
		printf("[MMDB] Player %d country: %s", playerid, country);
	else
		PrintMMDBError("query country", status);

	status = mmdb_get_text("asn", ip, "/autonomous_system_number", asn, sizeof(asn), required);
	if (status == MMDB_OK)
		printf("[MMDB] Player %d ASN: %s", playerid, asn);
	else
		PrintMMDBError("query ASN", status);
	return 1;
}
