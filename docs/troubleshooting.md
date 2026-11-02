# Troubleshooting

Use the explicit MaxoraStatus returned by the failed call and Maxora_GetStatusMessage to explain it. Success is zero. There is no global last-error buffer.

| Symptom | Check |
| --- | --- |
| Component does not load | Use an x86 build for the x86 open.mp runtime. Inspect the server log and OS dependency loader diagnostics. On Windows install the compatible MSVC runtime if using dynamic CRT. On Linux verify 32-bit libc, libstdc++ and libatomic runtime dependencies. |
| Natives are undefined | Put Maxora binary in components, enable the Pawn component, use the v2 include, and recompile. A v1 DLL does not export the v2 API. |
| OPEN_FAILED | File paths resolve from the server working directory. Check file existence, regular-file status and permissions. Maxora does not download a missing database. |
| INVALID_DATABASE / UNSUPPORTED_DATABASE | Use a provider MMDB file rather than a compressed archive or HTML download error. Check provider hashes and supported format. File limit is 1 GiB. |
| NO_RECORD | The database does not have data for this address. This is distinct from malformed IP. |
| NO_FIELD / TYPE_MISMATCH | Inspect schema and type. Edition, geography and language availability vary by record. Use JSON Pointer, not old dot syntax. |
| INVALID_IP / UNSUPPORTED_FAMILY | Pass only a numeric address without brackets, port, CIDR or zone. Use an IPv6-capable database for IPv6. |
| BUFFER_TOO_SMALL | Allocate required cells, including NUL, and retry. The original destination is unchanged. |
| OUT_OF_RANGE | Use GetText for wide integers; check float limits. Do not coerce or silently truncate. |
| INVALID_HANDLE / WRONG_OWNER | Retain only live handles from the calling script. Release results once. Reload preserves the DB handle; unload invalidates it. |
| RESOURCE_LIMIT | Release snapshots promptly and unload unused databases. See owner/global and lifetime handle issuance limits. |
| DECODER_ERROR | Stop trusting that database file, verify source integrity, deploy an immutable replacement and reload. A successful open did not decode every value. |
| INVALID_ARGUMENT | Pass the exact signature, true buffer size, unpacked byte string, terminated bounded input, and distinct output references. |

The full status table is in [the API reference](api.md). Attach the exact component commit, open.mp version, OS/architecture, build toolchain and a redistributable reproducer when reporting defects. Do not attach licensed database contents or player IP logs without permission.
