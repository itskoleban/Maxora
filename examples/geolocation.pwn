#include <open.mp>
#include <omp_mmdb>

main() {}

public OnGameModeInit() {
	new MMDBStatus:asn = mmdb_load("asn", "scriptfiles/GeoLite2-ASN.mmdb");
	new MMDBStatus:city = mmdb_load("city", "scriptfiles/GeoLite2-City.mmdb");

	if (city != MMDB_OK || asn != MMDB_OK) {
		printf("[MMDB] Load status: city=%d asn=%d", _:city, _:asn);
	}

	return 1;
}

public OnPlayerConnect(playerid) {
	new ip[46], country[128], asn[40], required;

	GetPlayerIp(playerid, ip, sizeof(ip));

	if (mmdb_get_string("city", ip, "/country/names/en", country, sizeof(country), required) == MMDB_OK) {
		printf("[MMDB] %s country: %s", ip, country);
	}

	if (mmdb_get_text("asn", ip, "/autonomous_system_number", asn, sizeof(asn), required) == MMDB_OK) {
		printf("[MMDB] %s ASN: %s", ip, asn);
	}

	return 1;
}
