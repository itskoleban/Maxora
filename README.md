# Maxora

[![Release](https://img.shields.io/github/v/release/itskoleban/Maxora?style=flat-square)](https://github.com/itskoleban/Maxora/releases/latest)
[![Build Status](https://img.shields.io/github/actions/workflow/status/itskoleban/Maxora/release.yml?style=flat-square)](https://github.com/itskoleban/Maxora/actions)

Maxora is a highly optimized [open.mp](https://open.mp/) component for querying [MaxMind DB (`.mmdb`)](https://maxmind.github.io/MaxMind-DB/) databases directly from Pawn scripts.

It provides highly performant IP geolocation lookups (such as Country, City, ASN, and ISP) for high-concurrency open.mp servers without introducing garbage collection overhead to the main server thread.

## Table of Contents

- [Features](#features)
- [Architecture / Design Overview](#architecture--design-overview)
- [Requirements](#requirements)
- [Installation](#installation)
- [Usage](#usage)
- [API Reference](#api-reference)
- [Error Handling / Troubleshooting](#error-handling--troubleshooting)
- [Development & Testing](#development--testing)

---

## Features

- **Performance (Memory-Mapped I/O)**: Uses `MMDB_MODE_MMAP` for extremely fast `O(depth)` lookups directly from the OS disk cache.
- **Optimized Memory Usage**: Executes hot paths (reading from AMX, preparing paths, and querying the database) largely on the C++ stack using fixed-size buffers and `thread_local` memory arrays to minimize dynamic heap allocations per query.
- **Memory Safety**: Utilizes open.mp's native `StringView` and strict bounds checking to prevent buffer overflows during AMX virtual machine interaction.
- **Runtime Loading**: You can load new `.mmdb` databases safely at runtime via Handles. Invalid or corrupted databases are rejected gracefully.
- **Dynamic Path Navigation**: Natively queries any arbitrary depth hierarchy (e.g., `"country.names.en"`) in the DB tree. Supports up to 127 path segments. Custom delimiter `/` is supported.

## Architecture / Design Overview

Maxora is built strictly against the modern `IComponent` interface of the open.mp SDK.

- **Pawn Natives**: Bridges the Pawn AMX machine and the C++ backend (`src/natives.cpp`). Extracts arguments using pointer references to prevent null-termination vulnerabilities, and utilizes `thread_local` arrays for efficient path parsing.
- **Backend Engine**: Wraps the standard C API of `libmaxminddb` (`src/maxmind_store.cpp`). It manages the lifecycle of `MMDB_s` contexts using a handle-based architecture, allowing multiple databases to be loaded and queried simultaneously.
- **In-Place Parser**: The internal `PreparePath` function mutates the path string in-place on the stack, constructing a node-pointer array efficiently without relying on heavy heap construction.

## Requirements

- **Server**: open.mp (x86 architecture).
- **Supported OS**: Windows, Linux.
- **Database**: Valid MaxMind Database (`.mmdb` format).
- **Build Requirements**: CMake 3.19+, C++17 Compiler (MSVC 19+, GCC, or Clang), `gcc-multilib` / `g++-multilib` for 32-bit Linux builds.

## Installation

1. Download the latest compiled binaries (`Maxora.dll` for Windows or `Maxora.so` for Linux) from the [Releases page](https://github.com/itskoleban/Maxora/releases/latest).
2. Place the binary in your open.mp `components` directory (open.mp will automatically load it).
3. Download a `.mmdb` database (e.g., [GeoLite2 Free Geolocation Data](https://dev.maxmind.com/geoip/geolite2-free-geolocation-data)) and place it in the server's root directory.
4. Move `include/maxora.inc` to your `pawno/include` folder.
5. Include the library in your script:

```pawn
#include <maxora>
```

## Usage

All configurations are handled programmatically through the Pawn API via `MMDB_Load`. No external configuration files are needed.

```pawn
#include <open.mp>
#include <maxora>

new MMDB:CityDB;

main() {
	print("Maxora example script loaded.");
}

public OnGameModeInit() {
	// The path resolves from your server's root directory.
	// You can load multiple databases by assigning them to different variables.
	CityDB = MMDB_Load("scriptfiles/GeoLite2-Country.mmdb");

	if (CityDB != INVALID_MMDB_HANDLE) {
		print("[MMDB] Database loaded successfully.");
	} else {
		new err[128];
		MMDB_GetLastError(err, sizeof(err));
		printf("[MMDB] Failed to load database. Reason: %s", err);
	}

	return 1;
}

public OnPlayerConnect(playerid) {
	new ip[16], country[64];

	GetPlayerIp(playerid, ip, sizeof(ip));

	// Querying a localized English country name using stock helpers
	if (MMDB_GetCountryName(CityDB, ip, country, sizeof(country), MMDB_LANG_ENGLISH) == MMDB_SUCCESS) {
		printf("[MMDB] Player %d connected | IP: %s | Country: %s", playerid, ip, country);
	} else {
		new err[128];
		MMDB_GetLastError(err, sizeof(err));
		printf(
			"[MMDB] Player %d connected | IP: %s | Country: Unknown (Reason: %s)",
			playerid,
			ip,
			err
		);
	}

	return 1;
}
```

## API Reference

All data extraction natives return a predefined status macro (e.g. `MMDB_SUCCESS`, `MMDB_NOT_FOUND`). Data is assigned safely to reference variables (`&dest`).

### Return Codes

| Macro                  | Value | Description                                                               |
| ---------------------- | ----- | ------------------------------------------------------------------------- |
| `MMDB_SUCCESS`         | `1`   | The query was executed successfully and data was assigned to the reference. |
| `MMDB_ERROR`           | `0`   | A fatal error occurred (e.g. malformed IP address, libmaxminddb failure). |
| `MMDB_NOT_FOUND`       | `-1`  | The IP exists, but the requested data field does not.                     |
| `MMDB_INVALID_HANDLE`  | `-2`  | The provided database handle is invalid or was not loaded.                |
| `MMDB_INVALID_PARAMS`  | `-3`  | Invalid parameters passed to the native function (e.g., negative size).   |
| `MMDB_TYPE_MISMATCH`   | `-4`  | The data type in the DB does not match the native called (e.g., Float on String). |

### Core Natives

| Native                                 | Description                                                          |
| -------------------------------------- | -------------------------------------------------------------------- |
| `MMDB:MMDB_Load(const filename[])`     | Loads an `.mmdb` file into memory. Returns a handle on success.      |
| `MMDB_Unload(MMDB:handle)`             | Safely unloads the database and frees OS file descriptors.           |
| `bool:MMDB_IsLoaded(MMDB:handle)`      | Returns `true` if a database is actively mapped in memory.           |
| `bool:MMDB_GetLastError(dest[], size)` | Extracts the last internal error message generated by the component. |

### Data Extraction Natives

| Native                                                                | Description                                                          |
| --------------------------------------------------------------------- | -------------------------------------------------------------------- |
| `MMDB_GetString(MMDB:handle, const ip[], const path[], dest[], size)` | Extracts a UTF-8 string from the given JSON-like path.               |
| `MMDB_GetInt(MMDB:handle, const ip[], const path[], &dest)`           | Extracts a 16, 32, or 64-bit integer (safe against cell bounds).     |
| `MMDB_GetFloat(MMDB:handle, const ip[], const path[], &Float:dest)`   | Extracts a single or double-precision float.                         |
| `MMDB_GetBool(MMDB:handle, const ip[], const path[], &bool:dest)`     | Extracts a boolean state.                                            |
| `MMDB_HasField(MMDB:handle, const ip[], const path[], &bool:exists)`  | Checks if a path exists within the IP's block without extracting it. |
| `MMDB_GetNetmask(MMDB:handle, const ip[], &dest)`                     | Retrieves the IPv4/IPv6 routing prefix (netmask) for the given IP.   |

### Helper Functions (Stocks)

These wrappers are provided in `maxora.inc`. They only compile into your script if utilized.

| Stock Helper                                                         | Description                                           |
| -------------------------------------------------------------------- | ----------------------------------------------------- |
| `MMDB_GetCountryCode(MMDB:handle, const ip[], dest[], size)`         | Retrieves the ISO 2-letter country code (e.g., "US"). |
| `MMDB_GetCountryName(MMDB:handle, const ip[], dest[], size, lang[])` | Retrieves the localized country name.                 |
| `MMDB_GetRegionCode(MMDB:handle, const ip[], dest[], size)`          | Retrieves the region/state ISO code (e.g., "CA").     |
| `MMDB_GetRegionName(MMDB:handle, const ip[], dest[], size, lang[])`  | Retrieves the localized region/state name.            |
| `MMDB_GetCityName(MMDB:handle, const ip[], dest[], size, lang[])`    | Retrieves the localized city name.                    |
| `MMDB_GetASN(MMDB:handle, const ip[], &dest)`                        | Retrieves the Autonomous System Number.               |
| `MMDB_GetISP(MMDB:handle, const ip[], dest[], size)`                 | Retrieves the Internet Service Provider name.         |

#### Language Constants

`MMDB_LANG_ENGLISH` ("en"), `MMDB_LANG_SPANISH` ("es"), `MMDB_LANG_GERMAN` ("de"), `MMDB_LANG_FRENCH` ("fr"), `MMDB_LANG_JAPANESE` ("ja"), `MMDB_LANG_PORTUGUESE` ("pt-BR"), `MMDB_LANG_RUSSIAN` ("ru"), `MMDB_LANG_CHINESE` ("zh-CN").

### Database Compatibility

MaxMind offers both free and paid database families. The functions you can successfully call depend on the database you loaded:

| Database Type        | Supported Helpers                                          | Notes                                                                 |
| -------------------- | ---------------------------------------------------------- | --------------------------------------------------------------------- |
| **GeoLite2-City**    | `GetCountryCode/Name`, `GetRegionCode/Name`, `GetCityName` | The most complete geolocation DB. Includes country, region, and city. |
| **GeoLite2-Country** | `GetCountryCode/Name`                                      | Will return `false` for city and region queries.                      |
| **GeoLite2-ASN**     | `GetASN`, `GetISP`                                         | Contains routing data. Does **not** contain any country or city data. |

> [!IMPORTANT]
> This plugin has only been developed and tested using the free **GeoLite2** database family. It may work with paid variants like GeoIP2, but such usage remains completely untested.

_(Note: Functions such as `MMDB_GetNetmask` and generic path-based lookups like `MMDB_GetString` work universally on any database that actually contains the requested field path)._

## Error Handling / Troubleshooting

Maxora safely catches internal exceptions and exposes them via `MMDB_GetLastError`.
Known internal errors:

- `Database not loaded.`: Attempted to query data before calling `MMDB_Load`.
- `Field not found.`: The specific JSON path requested does not exist for the given IP block.
- `Path contains too many segments (limit 127).`: The requested path exceeds the delimiter node limit.
- `Invalid AMX address or filename too long.`: Memory issue within the AMX environment during string retrieval.

## Development & Testing

1. Clone the repository recursively to fetch submodules:

```bash
git clone --recursive https://github.com/itskoleban/Maxora.git
```

2. Configure CMake:

```bash
cmake -S . -B build -A Win32 -DCMAKE_BUILD_TYPE=Release
```

3. Build the component:

```bash
cmake --build build --config Release
```

### Running Backend Tests

The project includes a standalone C++ test target named `maxora_test` to verify backend store behavior independently of the open.mp runtime.

1. Compile the `maxora_test` target via CMake.
2. Run the resulting executable located in `build/Release/maxora_test.exe` (or `build/maxora_test` on Linux).

## Build and Release Process

- **CI/CD**: Managed via GitHub Actions (`.github/workflows/release.yml`).
- Triggers on pushed version tags (`v*`).
- Compiles standard 32-bit `Maxora.dll` for Windows and `Maxora.so` for Linux, utilizing GCC `-m32` cross-compilation on Ubuntu.
- Automatically creates a GitHub Release containing both artifacts.

---

_This project relies on [libmaxminddb](https://github.com/maxmind/libmaxminddb) and the modern [open.mp SDK](https://github.com/openmultiplayer/open.mp)._
