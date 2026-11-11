# Installation

Use `omp-mmdb-2.0.0-windows-x86.zip` or `omp-mmdb-2.0.0-linux-x86.zip` to match the operating system of your x86 open.mp server. The integration suite targets open.mp v1.6.7.3218. Keep the official `Pawn.dll` or `Pawn.so` in `components/`. Copy the matching `MMDB.dll` or `MMDB.so` alongside it. Linux distributions use the exact filename `MMDB.so`.

Copy `include/omp_mmdb.inc` to your Pawn compiler's include path and recompile affected scripts. Copy your MMDB files to a server-readable directory. Example paths are relative to the server working directory.

Verify the archive checksum and use `scripts/verify_package.py` with the expected commit when qualifying a candidate. The package contains the binary, include, examples, documentation and dependency notices. Acquire and license production databases separately.

Run the server and check for the MMDB component startup message. During migration, remove the previous Maxora binary before installing MMDB because both share the component UID.
