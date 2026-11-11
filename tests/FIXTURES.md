# Synthetic MMDB fixtures

Tests and benchmarks read selected synthetic databases from
`lib/libmaxminddb/t/maxmind-db/test-data`. They contain test data, not a production
GeoIP or GeoLite distribution. The recursive Git submodule commits pin their
exact contents; see `git submodule status --recursive`.

Source: https://github.com/maxmind/MaxMind-DB

Copyright (c) 2013–2026 MaxMind, Inc. The upstream test-data software is dual
licensed under Apache-2.0 or MIT. This project uses those fixtures under the MIT
option. Retain the upstream README copyright and `LICENSE-MIT` when copying them.
The libmaxminddb reader is separately Apache-2.0 licensed; package its LICENSE
and NOTICE with release binaries.

The suite selects decoder, nested, IPv4/IPv6/mixed 24/28/32-bit tree, broken
pointer and broken tree fixtures. It also writes tiny malformed temporary files.
It does not exhaustively expand the complete fixture directory, which contains
deliberately hostile structures. An upstream fixture filename containing GeoIP
or GeoLite does not turn it into a licensed production database.

## Optional provider fixtures

The open.mp integration runner can also verify two provider schemas when the
uncompressed databases are supplied as `--dbip-city PATH` and
`--ipinfo-lite PATH`. These arguments extend the standard test from the
[build instructions](../docs/build.md). Each input is hashed in the resulting
evidence, and each provider check runs again after the gamemode restarts.

- The November 2026 DB-IP City Lite download from [DB-IP](https://db-ip.com/db/download/ip-to-city-lite)
  is available under CC BY 4.0. Its uncompressed file has 126,873,350 bytes
  and SHA-256 `5f64491f77c2c78b56cd83a88db4ea6c4666874322da38805498bbb754ccc735`.
  The script checks that `/country/iso_code` for `8.8.8.8` is `US`.
- The official [IPinfo Lite sample](https://github.com/ipinfo/sample-database/tree/3cf4e76d882af7b3beca24817841ad0a0a4fa1e9/IPinfo%20Lite)
  is available under CC BY 4.0. Its MMDB has 3,301 bytes and SHA-256
  `4f8a5824a59952a97684b256aeac632ac9fd2b1ecab15fedfe60fbf492e2fbb7`.
  The script checks the top-level `/country_code` and `/asn` fields for
  `1.0.0.1`, including rejection of the string ASN by the integer getter.

Both checks passed on [Windows x86 with open.mp v1.6.7.3218](../docs/validation/omp-mmdb-windows-x86/ci-windows-providers.json).
Standard CI uses the pinned synthetic MaxMind fixtures. Provider data is
supplied separately for these checks.
