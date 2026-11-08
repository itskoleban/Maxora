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
