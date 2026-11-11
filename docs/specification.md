# Database format and traversal

omp-mmdb reads the [MaxMind DB format](https://maxmind.github.io/MaxMind-DB/) through the pinned libmaxminddb 1.14.1 reader. It relies on the binary format and metadata. Country, city, ASN and other application fields have provider-defined names and types.

Open validates reader support, metadata, the search-tree extent and the data-section separator. The component restricts record decoding to the declared data section. Search-tree nodes and record contents are decoded on demand. A database that opens can still contain a corrupt record. The pinned reader also requires `languages` and `description` metadata when opening a file.

JSON Pointer traversal supports empty map keys, nested maps and arrays, and the `~0` and `~1` escapes. An empty path selects the record root. Map keys are literal tokens, including tokens that look like numbers. Array indices use canonical nonnegative decimal syntax. An out-of-bounds index or traversal through a scalar reports a missing field. Invalid array syntax reports `MMDB_INVALID_PATH`.

Container sizes come from the decoded header. Accessing an element of a corrupt container can still report a decoder error.

The component reads strings, booleans, signed and unsigned integers through 128 bits, floats, doubles, bytes, arrays and maps. Type checks and exact integer representations are described in the [API reference](api.md).

Enumerating arbitrary map keys or returning a complete container would require walking records and bounding both emitted data and skipped subtrees. The current API supports targeted paths and counts. A future enumeration or serialization design needs explicit budgets for nesting, total decoded elements, output bytes and work per server tick.
