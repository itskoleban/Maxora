# Input validation and operating assumptions

Pawn natives require the parameter block to start at the interpreter's saved stack position (`stk`). They validate the entire expected block before reading the count cell and check that the declared byte count matches the native's arity before reading argument values. The bridge rejects invalid cell offsets, counts and ranges. Each checked span must fit entirely within the logical data and heap region (`[0, hea)`) or the active stack (`[stk, stp)`).

For inline AMX storage, where `AMX::data` is null, native entry checks that `AMX::stp` is nonnegative and no greater than the image header's `stp - dat - sizeof(cell)`. The last subtraction accounts for the reserved stack sentinel. The bridge widens all operands to signed 64-bit integers before subtracting and checks the limit before forming the inline data pointer. With an intact header, this detects isolated increases of `AMX::stp` beyond the header-derived ceiling. Separate storage supplied through `AMX::data` uses the same parameter origin and logical range checks and bypasses the inline header check.

Supported open.mp deployments assume valid, trusted AMX images and coherent, live, correctly aligned storage supplied by the runtime. Logical range checks and the inline ceiling check depend on that contract. They cannot independently establish the physical extent of the backing allocation. Stale pointers, freed or inconsistent backing storage, invalid headers, and changes to both the header and registers that preserve the checked relationship fall outside this protection.

The component provides no isolation from other native plugins in the server process or protection against arbitrary memory corruption. Audit 2.1 found no route from ordinary Pawn arguments to access outside the physical allocation under the supported runtime contract. The residual risk is a conditional host contract violation. [Audit 2.1.2](https://github.com/itskoleban/omp-mmdb/issues/9#issuecomment-6834600907)

Inputs are bounded unpacked strings. Output arguments are written only after validation and a successful lookup or conversion, except for the required-length result on a short buffer. The native boundary contains C++ exceptions.

File paths are trusted deployment inputs. Use regular files on local storage and protect the database directory against writes or replacement by untrusted users. Publish a new complete file under a new filename before loading it. Keep every mapped file immutable and avoid writing or truncating it in place. Concurrent modifications to mapped files are outside the reader contract and may cause OS-level faults.

Open validates file layout and metadata. Individual records are decoded on demand through the pinned libmaxminddb reader. Corrupt input can fail at load time or return a decoder error at lookup time. Use provider checksums and an independently verified deployment source when available.

The Pawn bridge and database registry belong to the server thread. Only immutable mappings with owned references may cross threads in C++ tests. See [architecture](architecture.md) for lifecycle constraints.
