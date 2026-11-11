# omp-mmdb

## Introduction

omp-mmdb is a C++20 component for open.mp. Pawn scripts use local database names and numeric IP addresses to read fields from files supported by libmaxminddb. Paths select fields by schema, so the same API works with geographic, ASN and other MMDB datasets.

## Installation

Copy **MMDB.dll** (Windows x86) or **MMDB.so** (Linux x86) into the server's **components/** directory alongside the official Pawn component. Copy **include/omp_mmdb.inc** into your Pawn compiler's include directory and place your licensed databases in **scriptfiles/** or another server-readable directory. See [installation](docs/installation.md) for package verification and deployment details.

## Quick Start

Compile [examples/geolocation.pwn](examples/geolocation.pwn) with the open.mp Pawn includes. It loads the City and ASN databases, reads the connecting player's address and looks up country and network information.

~~~pawn
#include <open.mp>
#include <omp_mmdb>

main() {}

public OnGameModeInit()
{
	new MMDBStatus:city = mmdb_load("city", "scriptfiles/GeoLite2-City.mmdb");
	new MMDBStatus:asn = mmdb_load("asn", "scriptfiles/GeoLite2-ASN.mmdb");
	if (city != MMDB_OK || asn != MMDB_OK)
		printf("[MMDB] Load status: city=%d asn=%d", _:city, _:asn);
	return 1;
}

public OnPlayerConnect(playerid)
{
	new ip[46], country[128], asn[40], required;
	GetPlayerIp(playerid, ip, sizeof(ip));
	if (mmdb_get_string("city", ip, "/country/names/en", country, sizeof(country), required) == MMDB_OK)
		printf("[MMDB] %s country: %s", ip, country);
	if (mmdb_get_text("asn", ip, "/autonomous_system_number", asn, sizeof(asn), required) == MMDB_OK)
		printf("[MMDB] %s ASN: %s", ip, asn);
	return 1;
}
~~~

## Database Management

Use `mmdb_load(name, file)` to register a name and `mmdb_reload(name, file)` to replace its mapping. Failed reloads preserve the previous database. `mmdb_is_loaded` reports whether a name is present, and `mmdb_unload` releases it early. Names are case-sensitive and private to the calling script. Gamemode and filterscript unload release that script's databases.

## Querying Data

Pass a numeric IPv4 or IPv6 address and a JSON Pointer. For example, a dataset may use `/country/names/en` or `/country_code` for geographic data. `mmdb_has_record` checks coverage, and `mmdb_get_prefix` returns the matched network prefix in bits. `mmdb_get_text` preserves large integers as decimal and represents binary values as lowercase hexadecimal.

## Arrays and Maps

Use `/subdivisions/0/names/en` for the first element of a nested array. `""` selects the root, `~0` represents `~` and `~1` represents `/` within a key. `mmdb_get_type` identifies a container, `mmdb_get_size` counts its children and `mmdb_has_field` checks a path.

For unknown schemas, inspect the provider's field specification or inspect records offline with a database tool. The component bounds work on the server thread by exposing only field traversal and container inspection. See [format and traversal](docs/specification.md) for the enumeration and serialization trade-off.

## Error Handling

Every native returns `MMDBStatus:`. Compare it with `MMDB_OK`, which is zero. `mmdb_get_status_message` formats a status directly. On `MMDB_BUFFER_TOO_SMALL`, `required` includes the terminating cell and the destination is unchanged. See [status and output contracts](docs/api.md#error-handling).

## Asynchronous Reloading

The component currently exposes synchronous `mmdb_reload`. The open.mp Pawn SDK has no lifetime guard for a callback that triggers immediate unload or reload of its own script. A future asynchronous callback requires a server guarantee that the script stays alive until execution returns. [Lifecycle details](docs/architecture.md#asynchronous-reloading).

## API Reference

The [public include](include/omp_mmdb.inc) declares every native and tag. See the [API reference](docs/api.md) for argument order, return states and conversions, and the [migration guide](docs/migration.md) for changes from Maxora 1.x and 2.0 RC.

## Building and Testing

Initialize recursive submodules and use CMake 3.24 or newer. The component builds for Windows x86 and Linux x86. The CI suite also builds a core-only host configuration, compiles Pawn examples, exercises a packaged component in open.mp and runs sanitizers and fuzzers. [Build commands](docs/build.md) · [Release qualification](docs/production-readiness.md).

## Performance and Limitations

Lookups and reloads run on the Pawn execution thread. Memory mapping permits demand paging, so cold pages and I/O can stall a tick. Stage immutable local files and reload during a low-traffic window. x86 maps the whole file and can fail below the reader's 2,147,483,647-byte ceiling, especially when old and new mappings coexist. [Measurements and reproduction](docs/performance.md).

## License

[MIT](LICENSE). The package includes notices for its third-party dependencies. Maxora's Git history and earlier version tags remain available.
