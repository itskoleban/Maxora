# Troubleshooting

- If open.mp fails to resolve `mmdb_*` natives, verify that the x86 binary is in `components/`, the official Pawn component is available and the script was compiled with `omp_mmdb.inc`.
- For `MMDB_NOT_LOADED`, check the spelling, case and load status in the script making the query. Each script owns its own namespace.
- For `MMDB_OPEN_FAILED`, verify the path against the server working directory, check file permissions and confirm the file can be mapped in the process's available address space.
- For `MMDB_NO_FIELD`, inspect the provider's schema, path escaping and array index. `MMDB_NO_RECORD` means the address matched no network.
- For `MMDB_BUFFER_TOO_SMALL`, supply a destination of at least the returned number of cells, including the terminator. `MMDB_OUT_OF_RANGE` for an integer field can be handled with `mmdb_get_text`.
- For inconsistent reads or mapping faults, check how database updates are deployed. Replace an immutable mapping with `mmdb_reload` and a new file rather than truncating the mapped file.
