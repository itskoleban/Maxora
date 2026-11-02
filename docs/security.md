# Security considerations

The trust boundary includes Pawn native arguments and externally supplied database files. The component validates numeric IP input without DNS, bounds path parsing, checks AMX memory spans, enforces output capacity and checked numeric conversions, and returns explicit statuses. Handle generation and script ownership prevent stale-handle reuse and cross-script resource control. Lookup snapshots retain database mappings through reload/unload.

The service runs synchronously on the open.mp main thread. Concurrent callers are unsupported. No background workers, network requests or global last-error state are introduced. Script resource limits bound live database/lookup handles. Database open and filesystem access can block; perform updates and initial loads outside latency-sensitive player callbacks where possible.

libmaxminddb is the database decoder. Opening validates metadata and basic file structure, not authenticity or every lazily decoded record. Use trusted database sources and separately verify download signatures or checksums. Mapped-file truncation can cause an operating-system access fault; never modify mapped files in place. Keep versioned database files until all active lookup snapshots release them.

Scripts execute within the server trust model: the API accepts operator-provided local filenames and does not implement a filesystem sandbox. Do not expose arbitrary database paths to players. Protect server/database directory permissions. Treat location data as approximate information, and avoid logging unnecessary IP addresses or making security authorization decisions from geolocation alone.

Dependencies and GitHub Actions are pinned by Git commit where practical. Release publication uses minimum token scope, verifies package hashes, and requires commit-bound evidence. A successful build, sanitizer run or source review alone is not a claim of complete security. Report suspected problems privately to the repository owner; do not attach license-restricted databases or personal IP logs to public reports.
