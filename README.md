# Maxora

[![Release](https://img.shields.io/github/v/release/itskoleban/Maxora?style=flat-square)](https://github.com/itskoleban/Maxora/releases/latest)
[![Build Status](https://img.shields.io/github/actions/workflow/status/itskoleban/Maxora/release.yml?style=flat-square)](https://github.com/itskoleban/Maxora/actions)

Maxora is a highly optimized, zero-allocation [open.mp](https://open.mp/) component for querying [MaxMind DB (`.mmdb`)](https://maxmind.github.io/MaxMind-DB/) databases directly from Pawn scripts.

It provides instant IP geolocation lookups (such as Country, City, ASN, and ISP) for high-concurrency open.mp servers without introducing latency or garbage collection overhead to the main server thread.

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

- **High Performance (Memory-Mapped I/O)**: Uses `MMDB_MODE_MMAP` for instant `O(depth)` lookups directly from the OS disk cache.
- **Zero Heap Allocations**: Executes hot paths (reading from AMX, preparing paths, and querying the database) entirely on the C++ stack using fixed-size buffers and `thread_local` memory arrays. Performs 0 dynamic heap allocations per query.
- **Memory Safety Guarantee**: Utilizes open.mp's native `StringView` and strict bounds checking to prevent buffer overflows during AMX virtual machine interaction.
- **Atomic Hot-Swapping**: Reloads the `.mmdb` database safely at runtime. Invalid or corrupted databases are rejected gracefully while preserving the existing database instance.
- **Dynamic Path Navigation**: Natively queries any arbitrary depth hierarchy (e.g., `"country.names.en"`) in the DB tree. Supports up to 127 path segments. Custom delimiter `/` is supported.

## Architecture / Design Overview

Maxora is built strictly against the modern `IComponent` interface of the open.mp SDK.

- **Pawn Natives**: Bridges the Pawn AMX machine and the C++ backend (`src/natives.cpp`). Extracts arguments using pointer references and limits to prevent null-termination vulnerabilities.
- **Backend Engine**: Wraps the standard C API of `libmaxminddb` with C++ thread-local storage (`src/maxmind_store.cpp`). It manages the lifecycle of the `MMDB_s` context to provide atomic reads and safe swapping.
- **Zero-Allocation Parser**: The internal `PreparePath` function mutates the path string in-place on the stack, constructing a node-pointer array without relying on heap construction.

## Requirements

- **Server**: open.mp (x86 architecture).
- **Supported OS**: Windows, Linux.
- **Database**: Valid MaxMind Database (`.mmdb` format).
- **Build Requirements**: CMake 3.19+, C++17 Compiler (MSVC 19+, GCC, or Clang), `gcc-multilib` / `g++-multilib` for 32-bit Linux builds.

## Installation

1. Download the latest compiled binaries (`maxora.dll` for Windows or `libmaxora.so` for Linux) from the [Releases page](https://github.com/itskoleban/Maxora/releases/latest).
2. Place the binary in your open.mp `components` directory.
3. Add `maxora` to the `components` section of your `config.json`.
4. Download a `.mmdb` database (e.g., [GeoLite2 Free Geolocation Data](https://dev.maxmind.com/geoip/geolite2-free-geolocation-data)) and place it in the server's root directory.
5. Move `include/maxora.inc` to your `pawno/include` folder.
6. Include the library in your script:
   ```pawn
   #include <maxora>
   ```

## Usage

All configurations are handled programmatically through the Pawn API via `MMDB_Load`. No external configuration files are needed.

```pawn
#include <open.mp>
#include <maxora>

public OnGameModeInit() {
	// The path resolves from your server's root directory.
	// You can place it anywhere, for example inside scriptfiles:
	if (MMDB_Load("scriptfiles/GeoLite2-City.mmdb")) {
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
	if (MMDB_GetCountryName(ip, country, sizeof(country), MMDB_LANG_ENGLISH)) {
		printf("[MMDB] Player %d connected | IP: %s | Country: %s", playerid, ip, country);
	} else {
		new err[128];
		MMDB_GetLastError(err, sizeof(err));
		printf("[MMDB] Player %d connected | IP: %s | Country: Unknown (Reason: %s)", playerid, ip, err);
	}

	return 1;
}
```

## API Reference

All data extraction natives return a boolean indicating success or failure. Data is assigned safely to reference variables (`&dest`).

### Core Natives

| Native                                 | Description                                                          |
| -------------------------------------- | -------------------------------------------------------------------- |
| `bool:MMDB_Load(const filename[])`     | Loads an `.mmdb` file into memory. Returns `true` on success.        |
| `MMDB_Unload()`                        | Safely unloads the database and frees OS file descriptors.           |
| `bool:MMDB_IsLoaded()`                 | Returns `true` if a database is actively mapped in memory.           |
| `bool:MMDB_GetLastError(dest[], size)` | Extracts the last internal error message generated by the component. |

### Data Extraction Natives

| Native                                                        | Description                                                          |
| ------------------------------------------------------------- | -------------------------------------------------------------------- |
| `bool:MMDB_GetString(const ip[], const path[], dest[], size)` | Extracts a UTF-8 string from the given JSON-like path.               |
| `bool:MMDB_GetInt(const ip[], const path[], &dest)`           | Extracts a 16, 32, or 64-bit integer (safe against cell bounds).     |
| `bool:MMDB_GetFloat(const ip[], const path[], &Float:dest)`   | Extracts a single or double-precision float.                         |
| `bool:MMDB_GetBool(const ip[], const path[], &bool:dest)`     | Extracts a boolean state.                                            |
| `bool:MMDB_HasField(const ip[], const path[], &bool:exists)`  | Checks if a path exists within the IP's block without extracting it. |
| `bool:MMDB_GetNetmask(const ip[], &dest)`                     | Retrieves the IPv4/IPv6 routing prefix (netmask) for the given IP.   |

### Helper Functions (Stocks)

These wrappers are provided in `maxora.inc`. They only compile into your script if utilized.

| Stock Helper                                                 | Description                                           |
| ------------------------------------------------------------ | ----------------------------------------------------- |
| `bool:MMDB_GetCountryCode(const ip[], dest[], size)`         | Retrieves the ISO 2-letter country code (e.g., "US"). |
| `bool:MMDB_GetCountryName(const ip[], dest[], size, lang[])` | Retrieves the localized country name.                 |
| `bool:MMDB_GetCityName(const ip[], dest[], size, lang[])`    | Retrieves the localized city name.                    |
| `bool:MMDB_GetASN(const ip[], &dest)`                        | Retrieves the Autonomous System Number.               |
| `bool:MMDB_GetISP(const ip[], dest[], size)`                 | Retrieves the Internet Service Provider name.         |

#### Language Constants

`MMDB_LANG_ENGLISH` ("en"), `MMDB_LANG_SPANISH` ("es"), `MMDB_LANG_GERMAN` ("de"), `MMDB_LANG_FRENCH` ("fr"), `MMDB_LANG_JAPANESE` ("ja"), `MMDB_LANG_PORTUGUESE` ("pt-BR"), `MMDB_LANG_RUSSIAN` ("ru"), `MMDB_LANG_CHINESE` ("zh-CN").

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
- Compiles standard 32-bit `maxora.dll` for Windows and `libmaxora.so` for Linux, utilizing GCC `-m32` cross-compilation on Ubuntu.
- Automatically creates a GitHub Release containing both artifacts.

---

_This project relies on [libmaxminddb](https://github.com/maxmind/libmaxminddb) and the modern [open.mp SDK](https://github.com/openmultiplayer/open.mp)._
