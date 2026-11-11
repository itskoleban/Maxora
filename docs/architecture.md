# Architecture

`MMDBComponent` integrates with the open.mp `Pawn` component. It registers the native table at AMX load and removes the script's namespace at AMX unload. The component retains the legacy UID to let open.mp detect duplicate installations. Its reset hook leaves active namespaces intact so filterscripts keep their databases across a gamemode restart.

`Service` is confined to the server thread. It maps a live AMX address to that script's namespace and keeps immutable, RAII-managed database mappings. Pointer reuse is safe because unload erases the namespace before the next attachment. No script identifier is issued or incremented. The registry grows with active scripts and databases.

A native call validates the AMX parameter block and reads bounded inputs. It resolves the current database, parses the IP, looks up a record and extracts the requested field. A call-local `Lookup` pins the immutable database until the native returns, including while a string view is copied back to Pawn. Its lifetime never crosses the native boundary. The same immutable snapshot can be read on multiple C++ threads if each reader owns a reference. Concurrent access to the Pawn VM or `Service` is outside the contract.

Reload opens and validates the replacement before changing the map entry. A failed open or validation leaves the original entry intact. Releasing the last reference closes the mapping, and destroying the component drops every namespace. Lookup does not increment a counter or allocate a permanent resource.

## Script unload and server shutdown

During ordinary script unload in open.mp v1.6.7.3218, including removal of the old gamemode during `gmx`, Pawn runs the script's exit callback before dispatching `onAmxUnload`. MMDB removes the owner's namespace at that notification. An active lookup can keep the mapping alive until its last reference is released. [Ordinary unload and reset](https://github.com/openmultiplayer/open.mp/blob/3d20f3dc0e26917264cdc8585c65913e080a143c/Server/Components/Pawn/Manager/Manager.cpp#L666-L751)

During full server shutdown in that runtime, open.mp notifies components through `onFree` before freeing each component, and Pawn runs `OnGameModeExit` and `OnFilterScriptExit` during its destruction. MMDB clears its namespaces when it disconnects from Pawn or is itself destroyed, so the callbacks can run after the natives have become unavailable. Scripts should rely on automatic cleanup because the public contract leaves native availability during these callbacks unspecified at full shutdown. [Component teardown order](https://github.com/openmultiplayer/open.mp/blob/3d20f3dc0e26917264cdc8585c65913e080a143c/Server/Source/core_impl.hpp#L282-L293) · [Pawn shutdown callbacks](https://github.com/openmultiplayer/open.mp/blob/3d20f3dc0e26917264cdc8585c65913e080a143c/Server/Components/Pawn/Manager/Manager.cpp#L58-L75)

## Asynchronous reloading

The supported open.mp runtime dispatches console commands inline. A Pawn native can call `SendRconCommand` and reach a script unload or reload while the VM is still executing a public. The SDK offers load and unload notifications and a borrowed script pointer, without a lifetime lease across the call. Checking the pointer before invocation would cover unloads between ticks and leave reentrant unload during execution unresolved.

The component exposes `mmdb_reload` until open.mp provides a lifetime guarantee such as deferring destruction until a running public returns. At that point a worker can prepare an immutable mapping, while the server thread verifies ownership, commits and delivers the callback. The relevant upstream paths are the [Pawn native](https://github.com/openmultiplayer/open.mp/blob/v1.6.7.3218/Server/Components/Pawn/Scripting/Core/Natives.cpp), [console dispatcher](https://github.com/openmultiplayer/open.mp/blob/v1.6.7.3218/Server/Components/Console/console_impl.hpp), and [script manager](https://github.com/openmultiplayer/open.mp/blob/v1.6.7.3218/Server/Components/Pawn/Manager/Manager.cpp).
