# Maxora

Maxora is a fast and robust [open.mp](https://open.mp/) component for querying MaxMind DB (`.mmdb`) databases directly from Pawn scripts.

> [!WARNING]
> **Development Status**
>
> This project is currently in the **development phase**. The API is subject to change and may contain bugs. Rigorous testing is highly recommended before deploying to production servers.

## Features

- 🚀 **High Performance**: Uses Memory-Mapped I/O for instant lookups (`O(depth)`) with minimal overhead.
- 🛡️ **Memory Safety**: Implements strict bounds checking, preventing buffer overflows when interacting with the AMX virtual machine.
- 🔄 **Safe Hot-Swap (Atomic Reload)**: Reloads the database (`.mmdb`) on the fly. If the new file is corrupted or inaccessible, the plugin will keep the previous database alive without crashing the server.
- 🌐 **Dynamic Path Support**: Natively queries any hierarchy (e.g., `"country.names.en"`) in the MaxMind DB. You can also start the path with `/` to use it as an alternative delimiter if your keys contain literal dots (e.g., `"/domain.com/names"`).
- 🧩 **open.mp Architecture**: Built using the modern `IComponent` interface from the native open.mp SDK.

## Pawn API

The native declarations can be found in `include/maxora.inc`. To prevent ambiguity issues with numeric return values (such as failing vs returning `0`), most data extraction natives return a `bool` (indicating success or failure) and output the requested value into a reference variable (`&dest`).

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

// Helpers for common queries (provided as Pawn stocks for convenience)
stock bool:MMDB_GetCountryCode(const ip[], dest[], size = sizeof(dest));
stock bool:MMDB_GetCountryName(const ip[], dest[], size = sizeof(dest), const lang[] = "en");
stock bool:MMDB_GetCityName(const ip[], dest[], size = sizeof(dest), const lang[] = "en");
stock bool:MMDB_GetASN(const ip[], &dest);
stock bool:MMDB_GetISP(const ip[], dest[], size = sizeof(dest));

// Error debugging
native bool:MMDB_GetLastError(dest[], size = sizeof(dest));
```

## Compilation

**Requirements**:

- CMake 3.5 or higher
- C++ Compiler (MSVC 19+, GCC, or Clang) with C++17 support.

```bash
cmake -S . -B build
cmake --build build --config Release
```

The compiled component will be available in the `build/Release` directory (Windows) or `build` (Linux) under the name `maxora.dll` / `libmaxora.so`. Place it in your open.mp `components` folder.

## Dependencies

This component statically includes and builds against the following dependencies:

- [libmaxminddb](https://github.com/maxmind/libmaxminddb)
- [open.mp SDK](https://github.com/openmultiplayer/open.mp)
