# MaxMind MMDB field paths

This page provides a practical **JSON Pointer field-path reference** for MaxMind's binary GeoIP and GeoLite databases when using omp-mmdb. These are paths *inside an MMDB record*, not filesystem paths. Load the corresponding database first, then pass its script-local name, a numeric IPv4/IPv6 address and the field path to a query native.

**Important:** Field availability depends on the database product, edition, release and IP address. Not every database supports every path, and fields with missing or undefined values may be omitted. This is a curated quick reference, **not** an exhaustive or frozen copy of MaxMind's schemas. Consult the linked official binary-database references for the full, current field definitions.

## Path syntax and native types

- omp-mmdb uses JSON Pointer paths, not the dot syntax used by older Maxora APIs: `/country/names/en` rather than `country.names.en`.
- Paths start with `/`; `""` selects the root record. Arrays use zero-based indices (`/subdivisions/0/names/en`). A pointer token containing `~` uses `~0`; one containing `/` uses `~1`.
- Paths are limited to 1,024 bytes and 32 tokens. The key names and structure must match the actual database record.
- Use `mmdb_get_string` for MMDB strings, `mmdb_get_float` for double/float fields, `mmdb_get_bool` for booleans, and `mmdb_get_int` for integer fields that fit a signed 32-bit Pawn cell.
- Use `mmdb_get_text` to preserve the exact decimal value of unsigned integers, including `uint32` ASN and GeoNames identifiers that might exceed a signed Pawn cell. `mmdb_get_int` can still be used when the value is known to fit.
- `mmdb_has_field` checks an optional path; `mmdb_get_type` checks its value type, and `mmdb_get_size` can inspect arrays such as `/subdivisions`. See the [Pawn API reference](api.md) for function signatures and return statuses.

## City (GeoLite2-City / GeoIP2-City)

These paths are for the City MMDB schema. Some also appear in Country and Enterprise products; their presence is not guaranteed for every IP.

| Field | JSON Pointer | MMDB type | Pawn reader |
| --- | --- | --- | --- |
| Country name (English) | `/country/names/en` | string | `mmdb_get_string` |
| Country name (Spanish, if provided) | `/country/names/es` | string | `mmdb_get_string` |
| Country ISO code | `/country/iso_code` | string | `mmdb_get_string` |
| Country GeoNames ID | `/country/geoname_id` | uint32 | `mmdb_get_text` |
| Country in EU (true-only flag) | `/country/is_in_european_union` | boolean | `mmdb_get_bool` |
| Continent name (English) | `/continent/names/en` | string | `mmdb_get_string` |
| Continent code | `/continent/code` | string | `mmdb_get_string` |
| City name (English) | `/city/names/en` | string | `mmdb_get_string` |
| City GeoNames ID | `/city/geoname_id` | uint32 | `mmdb_get_text` |
| First subdivision name | `/subdivisions/0/names/en` | string | `mmdb_get_string` |
| First subdivision code | `/subdivisions/0/iso_code` | string | `mmdb_get_string` |
| Second subdivision name, if available | `/subdivisions/1/names/en` | string | `mmdb_get_string` |
| Postal code | `/postal/code` | string | `mmdb_get_string` |
| Approximate latitude | `/location/latitude` | double | `mmdb_get_float` |
| Approximate longitude | `/location/longitude` | double | `mmdb_get_float` |
| Approximate accuracy radius (km) | `/location/accuracy_radius` | uint16 | `mmdb_get_int` |
| IANA time zone | `/location/time_zone` | string | `mmdb_get_string` |
| Registered country code | `/registered_country/iso_code` | string | `mmdb_get_string` |
| Represented country code, if available | `/represented_country/iso_code` | string | `mmdb_get_string` |
| Anycast flag (true-only) | `/traits/is_anycast` | boolean | `mmdb_get_bool` |

The `subdivisions` array is ordered from the broadest to the most specific subdivision; index `0` is not necessarily the most detailed region. Check its size before querying another index. Localized names may be absent even when the corresponding location is present. Coordinates are approximate, should be considered together with `accuracy_radius` and must not be treated as street-address precision. A Pawn `Float` has less precision than an MMDB `double`.

**Official reference:** [GeoIP City binary database fields](https://dev.maxmind.com/geoip/docs/databases/city-and-country/city-binary/).

## Country (GeoLite2-Country / GeoIP2-Country)

Country databases contain country and continent fields, but not City-specific keys such as `/city/names/en`, `/postal/code` or `/location/latitude`.

| Field | JSON Pointer | MMDB type | Pawn reader |
| --- | --- | --- | --- |
| Country ISO code | `/country/iso_code` | string | `mmdb_get_string` |
| Country name (English) | `/country/names/en` | string | `mmdb_get_string` |
| Country GeoNames ID | `/country/geoname_id` | uint32 | `mmdb_get_text` |
| Continent code | `/continent/code` | string | `mmdb_get_string` |
| Continent name (English) | `/continent/names/en` | string | `mmdb_get_string` |
| Registered country code | `/registered_country/iso_code` | string | `mmdb_get_string` |
| Represented country code, if available | `/represented_country/iso_code` | string | `mmdb_get_string` |
| Represented country type, if available | `/represented_country/type` | string | `mmdb_get_string` |
| Anycast flag (true-only) | `/traits/is_anycast` | boolean | `mmdb_get_bool` |

**Official reference:** [GeoIP Country binary database fields](https://dev.maxmind.com/geoip/docs/databases/city-and-country/country-binary/).

## ASN (GeoLite2-ASN)

ASN is a separate network database, not a subkey of GeoLite2-City. Both paths are at the root of an ASN record.

| Field | JSON Pointer | MMDB type | Pawn reader |
| --- | --- | --- | --- |
| Autonomous system number | `/autonomous_system_number` | uint32 | `mmdb_get_text` |
| Autonomous system organization | `/autonomous_system_organization` | string | `mmdb_get_string` |

**Official reference:** [GeoLite ASN binary database fields](https://dev.maxmind.com/geoip/docs/databases/asn/binary/).

## ISP (GeoIP2-ISP)

The ISP database is a separate product; do not assume GeoLite2-ASN also contains ISP names.

| Field | JSON Pointer | MMDB type | Pawn reader |
| --- | --- | --- | --- |
| Autonomous system number | `/autonomous_system_number` | uint32 | `mmdb_get_text` |
| Autonomous system organization | `/autonomous_system_organization` | string | `mmdb_get_string` |
| ISP name | `/isp` | string | `mmdb_get_string` |
| Organization | `/organization` | string | `mmdb_get_string` |
| Mobile country code, if available | `/mobile_country_code` | string | `mmdb_get_string` |
| Mobile network code, if available | `/mobile_network_code` | string | `mmdb_get_string` |

**Official reference:** [GeoIP ISP binary database fields](https://dev.maxmind.com/geoip/docs/databases/isp/binary/).

## Other MaxMind binary databases

These products use different schemas and may require their own licensed datasets.

| Product | Example path | MMDB type | Pawn reader |
| --- | --- | --- | --- |
| GeoIP Domain | `/domain` | string | `mmdb_get_string` |
| GeoIP Connection Type | `/connection_type` | string | `mmdb_get_string` |
| GeoIP Anonymous IP | `/is_anonymous` | boolean | `mmdb_get_bool` |
| GeoIP Anonymous IP | `/is_anonymous_vpn` | boolean | `mmdb_get_bool` |
| GeoIP Anonymous IP | `/is_hosting_provider` | boolean | `mmdb_get_bool` |
| GeoIP Anonymous IP | `/is_public_proxy` | boolean | `mmdb_get_bool` |
| GeoIP Anonymous IP | `/is_residential_proxy` | boolean | `mmdb_get_bool` |
| GeoIP Anonymous IP | `/is_tor_exit_node` | boolean | `mmdb_get_bool` |

In the Anonymous IP database, these flags may be **absent** when they are not true. Do not interpret a missing boolean field as a guaranteed false classification without checking the provider's semantics.

Official binary references: [Domain](https://dev.maxmind.com/geoip/docs/databases/domain/binary/) · [Connection Type](https://dev.maxmind.com/geoip/docs/databases/connection-type/binary/) · [Anonymous IP](https://dev.maxmind.com/geoip/docs/databases/anonymous-ip/binary/).

For more specialized schemas, see MaxMind's official [database documentation index](https://dev.maxmind.com/geoip/docs/databases/), including Enterprise, Anonymous Plus, Residential Proxy, Static IP Score and User Count. The presence of a similar-looking field in a different product **does not** imply an identical path.

## Pawn examples

Load the appropriate files in the script before calling these natives (see the [Quick Start](../README.md#quick-start)).

~~~pawn
new country[128], required;
if (mmdb_get_string("city", ip, "/country/names/en", country, sizeof(country), required) == MMDB_OK) {
	printf("[MMDB] Country: %s", country);
}

new asn[40];
if (mmdb_get_text("asn", ip, "/autonomous_system_number", asn, sizeof(asn), required) == MMDB_OK) {
	printf("[MMDB] ASN: %s", asn);
}
~~~

For optional fields, check existence first, or handle `MMDB_NO_FIELD`. An IP not matched by the database produces `MMDB_NO_RECORD`. A path can be valid JSON Pointer syntax yet be absent from a given provider's schema. Do not substitute arbitrary defaults for missing data unless your application's semantics permit it.

~~~pawn
new bool:exists;
if (mmdb_has_field("city", ip, "/city/names/en", exists) == MMDB_OK && exists) {
	new city[128], required;
	if (mmdb_get_string("city", ip, "/city/names/en", city, sizeof(city), required) == MMDB_OK) {
    	printf("[MMDB] City: %s", city);
    }
}
~~~

Use **tabs** rather than spaces for indentation when incorporating these snippets into project Pawn source files.

## Inspecting a different MMDB schema

omp-mmdb is a general-purpose MMDB reader, not a fixed GeoIP schema adapter. A third-party provider such as DB-IP or IPinfo may use paths like `/country_code` or `/asn` instead of MaxMind's nested fields.

To discover actual fields, inspect a representative record with the database provider's documentation or MaxMind's [`mmdbinspect` utility](https://github.com/maxmind/mmdbinspect) offline. Use `mmdb_get_type`, `mmdb_get_size` and `mmdb_has_field` for bounded runtime checks; omp-mmdb does not expose arbitrary map-key enumeration through Pawn.

This reference describes **binary .mmdb record keys**, not MaxMind's CSV column names or web-service response formats. Field names may change or expand with provider releases.