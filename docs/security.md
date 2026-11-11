# Input validation and operating assumptions

Pawn natives validate the parameter block and output spans against the live AMX heap and stack. Inputs are bounded unpacked strings. Output arguments are written only after validation and a successful lookup or conversion, except for the required-length result on a short buffer. The native boundary contains C++ exceptions.

File paths are trusted deployment inputs. Use regular files on local storage and protect the database directory against writes or replacement by untrusted users. Publish a new complete file under a new filename before loading it. Keep every mapped file immutable and avoid writing or truncating it in place. Concurrent modifications to mapped files are outside the reader contract and may cause OS-level faults.

Open validates file layout and metadata. Individual records are decoded on demand through the pinned libmaxminddb reader. Corrupt input can fail at load time or return a decoder error at lookup time. Use provider checksums and an independently verified deployment source when available.

The Pawn bridge and database registry belong to the server thread. Only immutable mappings with owned references may cross threads in C++ tests. See [architecture](architecture.md) for lifecycle constraints.
