# Migrating from Maxora

Replace `Maxora.dll` or `Maxora.so` in `components/` with `MMDB.dll` or `MMDB.so`. Replace `#include <maxora>` with `#include <omp_mmdb>` and recompile all affected gamemodes and filterscripts.

| Previous API | omp-mmdb |
|---|---|
| Maxora 1.x `MMDB_Load` or 2.0 RC `Maxora_Load` returning or writing a handle | `mmdb_load("city", file)` returns `MMDBStatus:` |
| Database handle checks | `mmdb_is_loaded("city", loaded)` |
| 2.0 RC `Maxora_Lookup` and a subsequent field read | `mmdb_get_string("city", ip, path, destination, capacity, required)` |
| 2.0 RC `Maxora_Release` | Remove the call and the result variable |
| 1.x dot-delimited paths | Convert to JSON Pointers, such as `/country/names/en` |
| 2.0 RC negative array indexes such as `/array/-1` | Use `mmdb_get_size` and construct a zero-based index such as `size - 1` for the last item |
| Provider helpers | Use the field path from the provider's schema |
| 1.x last error | Check the native's return status and use `mmdb_get_status_message` |

Every native returns `MMDB_OK` (zero) on success. Maxora 1.x used a nonzero success value. Pass the database name and IP to each field read. Multiple queries for one IP perform independent lookups against the current mapping. Each script must load its own names, even when scripts read the same file.

Old AMX binaries depend on their original native names. Preserve the original component for deployments still running them. Source and assets for Maxora remain available from the historical tags.
