# Architecture

`MMDBComponent` integrates with the open.mp `Pawn` component. It registers the native table at AMX load and removes the script's namespace at AMX unload. The component retains the legacy UID to let open.mp detect duplicate installations. Its reset hook preserves live scripts because a mode change may reset components after the new gamemode's initialization.

`Service` is confined to the server thread. It maps a live AMX address to that script's namespace and keeps immutable, RAII-managed database mappings. Pointer reuse is safe because unload erases the namespace before the next attachment. No script identifier is issued or incremented. The registry grows with active scripts and databases.

A native call validates the AMX parameter block and reads bounded inputs. It resolves the current database, parses the IP, looks up a record and extracts the requested field. A call-local `Lookup` pins the immutable database until the native returns, including while a string view is copied back to Pawn. Its lifetime never crosses the native boundary. The same immutable snapshot can be read on multiple C++ threads if each reader owns a reference. Concurrent access to the Pawn VM or `Service` is outside the contract.

Reload opens and validates the replacement before changing the map entry. A failed open or validation leaves the original entry intact. Releasing the last reference closes the mapping, and destroying the component drops every namespace. Lookup does not increment a counter or allocate a permanent resource.

## Asynchronous reloading

The supported open.mp runtime dispatches console commands inline. A Pawn native can call `SendRconCommand` and reach a script unload or reload while the VM is still executing a public. The SDK offers load and unload notifications and a borrowed script pointer, without a lifetime lease across the call. Checking the pointer before invocation would cover unloads between ticks and leave reentrant unload during execution unresolved.

The component exposes `mmdb_reload` until open.mp provides a lifetime guarantee such as deferring destruction until a running public returns. At that point a worker can prepare an immutable mapping, while the server thread verifies ownership, commits and delivers the callback. The relevant upstream paths are the [Pawn native](https://github.com/openmultiplayer/open.mp/blob/v1.6.7.3218/Server/Components/Pawn/Scripting/Core/Natives.cpp), [console dispatcher](https://github.com/openmultiplayer/open.mp/blob/v1.6.7.3218/Server/Components/Console/console_impl.hpp), and [script manager](https://github.com/openmultiplayer/open.mp/blob/v1.6.7.3218/Server/Components/Pawn/Manager/Manager.cpp).
