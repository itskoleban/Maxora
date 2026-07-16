# Maxora

Maxora is a highly optimized, production-ready, zero-allocation [open.mp](https://open.mp/) component for querying [MaxMind DB (`.mmdb`)](https://maxmind.github.io/MaxMind-DB/) databases directly from Pawn scripts.

Designed for high-concurrency SA-MP/open.mp servers, Maxora provides instant IP geolocation lookups (such as Country, City, ASN, and ISP) without introducing latency or garbage collection overhead to the main server thread.

## Features

- 🚀 **High Performance**: Uses Memory-Mapped I/O (`MMDB_MODE_MMAP`) for instant lookups (`O(depth)`) with minimal overhead. The database is accessed directly from disk cache without loading the entire structure into RAM.
- ⚡ **Zero Heap Allocations**: The plugin executes its hot paths (reading from AMX, preparing paths, and querying the database) entirely on the C++ stack using fixed-size buffers and `thread_local` memory arrays. It performs **0 dynamic heap allocations** per query.
- 🛡️ **Memory Safety**: Implements strict bounds checking and utilizes open.mp's native `StringView` to prevent buffer overflows and out-of-bounds reads when interacting with the AMX virtual machine.
- 🔄 **Safe Hot-Swap (Atomic Reload)**: Reloads the database (`.mmdb`) on the fly. If the new file is corrupted or inaccessible, the plugin will gracefully reject it while keeping the previous database alive, preventing server crashes during live updates.
- 🌐 **Dynamic Path Support**: Natively queries any hierarchy (e.g., `"country.names.en"`) in the MaxMind DB. You can also start the path with `/` to use it as an alternative delimiter if your keys contain literal dots (e.g., `"/domain.com/names"`).
- 🧩 **Modern Architecture**: Built natively using the modern `IComponent` interface from the [open.mp SDK](https://github.com/openmultiplayer/open.mp).

## Installation

1. Download the latest compiled `maxora.dll` (Windows) or `libmaxora.so` (Linux) from the [Releases page](../../releases).
2. Place it in your open.mp `components` folder.
3. Add `maxora` to the `components` section of your `config.json`.
4. Include `maxora.inc` in your Pawn script.
5. Download a MaxMind database (e.g., [GeoLite2 Free Geolocation Data](https://dev.maxmind.com/geoip/geolite2-free-geolocation-data)) and place it in your server root or an accessible path.

## Pawn API

The native declarations and stock helpers can be found in `include/maxora.inc`. To prevent ambiguity issues with numeric return values, most data extraction natives return a `bool` (indicating success or failure) and output the requested value into a reference variable (`&dest`).

### Main Functions

```pawn
// Database loading, unloading, and inspection
native bool:MMDB_Load(const filename[]);
native MMDB_Unload();
native bool:MMDB_IsLoaded();

// Dynamic queries
native bool:MMDB_GetString(const ip[], const path[], dest[], size = sizeof(dest));
native bool:MMDB_GetInt(const ip[], const path[], &dest);
native bool:MMDB_GetFloat(const ip[], const path[], &Float:dest);
native bool:MMDB_GetBool(const ip[], const path[], &bool:dest);
native bool:MMDB_HasField(const ip[], const path[], &bool:exists);
native bool:MMDB_GetNetmask(const ip[], &dest);

// Error debugging
native bool:MMDB_GetLastError(dest[], size = sizeof(dest));
```

### Stock Helpers

The include file provides `stock` wrappers for the most common data fields. These helpers provide a simpler syntax and only compile into your script if you actually use them.

```pawn
stock bool:MMDB_GetCountryCode(const ip[], dest[], size = sizeof(dest));
stock bool:MMDB_GetCountryName(const ip[], dest[], size = sizeof(dest), const lang[] = MMDB_LANG_ENGLISH);
stock bool:MMDB_GetCityName(const ip[], dest[], size = sizeof(dest), const lang[] = MMDB_LANG_ENGLISH);
stock bool:MMDB_GetASN(const ip[], &dest);
stock bool:MMDB_GetISP(const ip[], dest[], size = sizeof(dest));
```

### Language Constants

When querying names (like Country or City), you can pass a language code. Maxora provides the following macros for convenience:

- `MMDB_LANG_ENGLISH` ("en")
- `MMDB_LANG_SPANISH` ("es")
- `MMDB_LANG_GERMAN` ("de")
- `MMDB_LANG_FRENCH` ("fr")
- `MMDB_LANG_JAPANESE` ("ja")
- `MMDB_LANG_PORTUGUESE` ("pt-BR")
- `MMDB_LANG_RUSSIAN` ("ru")
- `MMDB_LANG_CHINESE` ("zh-CN")

### Usage Example

```pawn
#include <a_samp>
#include <maxora>

main()
{
    // You can download this database from https://dev.maxmind.com/geoip/geolite2-free-geolocation-data
    if (MMDB_Load("GeoLite2-City.mmdb"))
    {
        print("Database loaded successfully!");

        new country[64], city[64];
        new const ip[] = "8.8.8.8";

        // Using stock helpers with Spanish language constant
        if (MMDB_GetCountryName(ip, country, sizeof(country), MMDB_LANG_SPANISH))
        {
            printf("Country (Spanish): %s", country);
        }

        if (MMDB_GetCityName(ip, city, sizeof(city), MMDB_LANG_ENGLISH))
        {
            printf("City (English): %s", city);
        }
        else
        {
            // If the IP doesn't have city data, print the internal error reason
            new err[128];
            MMDB_GetLastError(err, sizeof(err));
            printf("Failed to get city: %s", err);
        }
    }
}
```

## Compilation

**Requirements**:

- CMake 3.19 or higher
- C++ Compiler (MSVC 19+, GCC, or Clang) with C++17 support.

```bash
cmake -S . -B build
cmake --build build --config Release
```

The compiled component will be available in the `build/Release` directory (Windows) or `build` (Linux) under the name `maxora.dll` / `libmaxora.so`.

## Dependencies

This component statically includes and builds against the following dependencies:

- [libmaxminddb](https://github.com/maxmind/libmaxminddb) (Provides the C API for `.mmdb` files)
- [open.mp SDK](https://github.com/openmultiplayer/open.mp) (Provides the modern AMX bindings)
