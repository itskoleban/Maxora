# Performance and benchmark results

The Release core benchmarks measure the C++ path for selecting and reading an
MMDB field. open.mp dispatch, AMX validation and buffer copies are outside
these measurements. The [benchmark instructions](../benchmarks/README.md)
describe the fixtures, commands and metrics.

## Windows x86 baseline comparison

On November 10, 2026, both benchmarks were built with MinGW GCC 16.1.0 in
Release mode for Windows x86 on a 13th Gen Intel Core i5-13420H host.
The historical binary was built from `661b8381f6397e21be54dfe6bd2ed3c4e2922b43`
and uses Maxora 2.0's original handle table. Both used libmaxminddb 1.14.1
(`4a3d17a621c0ebcb81498fd34d313f67fd940ab5`) and its fixture submodule
(`dc32db5525f9c42ab7006ab3f13c69330b814dd1`). Each row reports medians across
five separately launched, paired trials with 5,000 measured operations per
trial. All checksums matched. Host activity was uncontrolled.

| Operation | Maxora mean / p99 | omp-mmdb mean / p99 |
| --- | ---: | ---: |
| Clock pair | 192 / 300 ns | 191 / 300 ns |
| IPv4 and one field | 792 / 1,200 ns | 872 / 1,200 ns |
| IPv6 and one field | 1,346 / 1,900 ns | 1,622 / 2,500 ns |
| Missing IPv4 record | 404 / 700 ns | 468 / 700 ns |
| Three nested fields with fresh record resolution | 2,177 / 3,200 ns | 3,295 / 4,700 ns |
| Retained old result versus three direct calls | 1,831 / 2,600 ns | 2,746 / 4,500 ns |
| One fresh lookup for three fields in both implementations | 2,430 / 3,500 ns | 2,111 / 3,600 ns |
| IPv4 across four databases | 739 / 1,000 ns | 997 / 1,300 ns |
| Load, query, replace, unload and read held snapshot | 1,256 / 2,633 µs | 1,161 / 2,715 µs |

The older API resolves an IP once when creating a result and amortizes that
search across field reads. The direct API resolves an IP for each call. The
two independently paired trials that measure three direct calls show
run-to-run variation. Reusing one temporary C++ lookup reduced the observed
mean and kept snapshot consistency for all three fields.

Median process working set for the IPv4 trial was 7,180,288 bytes for Maxora
and 7,278,592 bytes for omp-mmdb. These process readings include the C++
runtime and benchmark's sampling storage. In a separate 60.0004-second mixed
run, the benchmark completed 345,861 measured operations and 78,037 concurrent
reloads, with eight live databases at the end. The query-under-reload workload
measured 2,621 ns mean, 4,900 ns p99 and 33,200 ns p99.9. Peak process working
set was 10,461,184 bytes. The two-thread test publishes mappings independently
of open.mp. [Stress run JSONL](validation/omp-mmdb-windows-x86/windows-x86-stress.jsonl)

[Paired trial summary](validation/omp-mmdb-windows-x86/comparison-summary.json),
[paired raw measurements](validation/omp-mmdb-windows-x86/comparison-raw.jsonl)
and [external database measurements](validation/omp-mmdb-windows-x86/external-raw.jsonl)
retain the measurements. The Windows candidate executable SHA-256 was
`1cd2faa093614a69326cf94bbf807c31a4a2c3347af411c86a0228e70ab5f7f3`.
The baseline executable SHA-256 was
`830d164d01d3d2f17e893c157a851f7a258a266d1b5faf5abd344be831594557`.

## File size and first touch

| Windows x86 dataset | File bytes | Addresses | Mean | p99 | p99.9 | Working set after |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| MaxMind decoder fixture, `/uint16` | 3,188 | 1 | 951 ns | 1,900 ns | 3,400 ns | 7,786,496 B |
| Same fixture with sparse padding, `/uint16` | 1,073,758,208 | 1 | 985 ns | 1,700 ns | 5,500 ns | 7,892,992 B |
| DB-IP City Lite, `/country/iso_code` | 126,873,350 | 4,096 | 1,821 ns | 3,100 ns | 28,100 ns | 32,059,392 B |

Each row represents 100,000 measured lookups. The first two workloads read
the same record near the beginning of each file. The sparse result verifies
that a mapping larger than 1 GiB worked on this host. The recorded DB-IP warm
run visited all 4,096 sampled addresses before timing. It opened the mapping
in 3.93 ms. Its workload and schema differ from those of the synthetic file.

In 16 separate DB-IP processes, each with zero warm-up and one lookup after
mapping open, query latency ranged from 14.4 to 48.2 µs, with a median of
27.55 µs. The process page-fault counter increased by 11 to 21. Windows
combines soft and hard faults in that counter, and the system page cache was
uncontrolled. This verifies fresh-process first-touch latency on Windows.

For Linux x86, run the [Performance and short stress workflow](../.github/workflows/performance.yml)
with the `dbip_first_touch` input enabled. It downloads and verifies the same
DB-IP edition, flushes the file's writes and uses `POSIX_FADV_DONTNEED` on that
file before each first-touch process. It then pairs each first-touch lookup
with a warm lookup of the same address. The `dbip-first-touch-evidence`
artifact contains all 16 pairs, process major and minor fault counts, the
environment and the dataset hash. The cache hint is advisory. The major
fault counter describes the Linux process during the measurement window.

IP Geolocation by [DB-IP](https://db-ip.com), City Lite November 2026,
CC BY 4.0. The uncompressed file's SHA-256 was
`5f64491f77c2c78b56cd83a88db4ea6c4666874322da38805498bbb754ccc735`.
The uncompressed SHA-1 matched the [provider's release page](https://db-ip.com/db/download/ip-to-city-lite).
Generate the address sample using Python 3.13 and its `maxminddb==2.8.2`
package. From `random.Random(0x4D4D4442)`, draw `getrandbits(32)` and
interpret each value as an IPv4 address. Retain 4,096 unique addresses for
which `is_global` is true, `is_multicast` is false, and the database's
`country.iso_code` exists. Write them in acceptance order with LF newlines.
The list SHA-256 was
`3359d8845888fafcb0858b7337a5755a0eaa6ffd28c09f5b0c3dd84638a0b31e`.
Fresh-process runs used `--start-index` values `0, 251, …, 3765`.

The Windows x86 open.mp v1.6.7.3218 integration also checked both providers
before and after a gamemode restart. DB-IP City Lite returned `US` for
`8.8.8.8` at `/country/iso_code`. The official IPinfo Lite sample returned
`AU` and `AS13335` for `1.0.0.1` at `/country_code` and `/asn`. A typed
integer read of that string ASN returned `MMDB_TYPE_MISMATCH`.
[Provider integration evidence](validation/omp-mmdb-windows-x86/ci-windows-providers.json)

## Grouped field design

A future synchronous `mmdb_get_many` can accept a database name, numeric IP,
up to 16 paths, and one requested conversion per path. A fixed-width flat
cell matrix can carry the paths and text outputs, with explicit row strides,
plus separate scalar, length and per-field status arrays. A call-wide bound
of 16 KiB on decoded input and formatted output keeps both allocation and
copy work bounded. Names, addresses, every input row, every output span, and
output overlap are validated before any VM write. Per-field errors such as
`MMDB_NO_FIELD` and `MMDB_BUFFER_TOO_SMALL` remain visible. Malformed batch
arguments fail the call before output.

The implementation takes one mapping reference, resolves the IP once, and
evaluates each bounded path against that lookup. It stages formatted values
and status outputs before copying them back. One mapping reference makes all
fields consistent across replacement. This proposed contract still needs
Pawn ergonomics tests and mixed-type measurements that include marshalling
cost. The current public API consists of single-field calls.

The [historical Maxora performance review](validation/benchmark-review.md)
contains prior release-candidate evidence. A new 24-hour soak of the final
omp-mmdb candidate remains a separate release gate.
