#if defined _maxora_included
    #endinput
#endif
#define _maxora_included

/**
 * Loads a MaxMind DB file.
 * Returns true if successful, false otherwise.
 */
native bool:MMDB_Load(const filename[]);

/**
 * Unloads the currently loaded database.
 */
native MMDB_Unload();

/**
 * Checks if a database is currently loaded.
 */
native bool:MMDB_IsLoaded();

/**
 * Gets a string value from a generic path in the loaded database.
 * The path components are separated by dots, e.g. "country.names.en".
 */
native bool:MMDB_GetString(const ip[], const path[], dest[], size = sizeof(dest));

/**
 * Gets an integer value from a generic path.
 */
native bool:MMDB_GetInt(const ip[], const path[], &dest);

/**
 * Gets a float value from a generic path.
 */
native bool:MMDB_GetFloat(const ip[], const path[], &Float:dest);

/**
 * Gets a boolean value from a generic path.
 */
native bool:MMDB_GetBool(const ip[], const path[], &bool:dest);

/**
 * Checks if a specific field/path exists for the given IP.
 */
native bool:MMDB_HasField(const ip[], const path[], &bool:exists);

/**
 * Gets the netmask associated with the matched IP entry.
 */
native bool:MMDB_GetNetmask(const ip[], &dest);

/**
 * Gets the last error message from the plugin.
 */
native bool:MMDB_GetLastError(dest[], size = sizeof(dest));

// ----------------------------------------------------
// Supported Languages in MaxMind DBs
// ----------------------------------------------------
#define MMDB_LANG_ENGLISH      "en"
#define MMDB_LANG_GERMAN       "de"
#define MMDB_LANG_SPANISH      "es"
#define MMDB_LANG_FRENCH       "fr"
#define MMDB_LANG_JAPANESE     "ja"
#define MMDB_LANG_PORTUGUESE   "pt-BR"
#define MMDB_LANG_RUSSIAN      "ru"
#define MMDB_LANG_CHINESE      "zh-CN"

// ----------------------------------------------------
// Helpers for common database fields (City, ASN, ISP)
// ----------------------------------------------------

/**
 * Gets the ISO 2-letter country code.
 */
stock bool:MMDB_GetCountryCode(const ip[], dest[], size = sizeof(dest))
{
    return MMDB_GetString(ip, "country.iso_code", dest, size);
}

/**
 * Gets the country name (defaults to MMDB_LANG_ENGLISH).
 */
stock bool:MMDB_GetCountryName(const ip[], dest[], size = sizeof(dest), const lang[] = MMDB_LANG_ENGLISH)
{
    new path[64];
    format(path, sizeof(path), "country.names.%s", lang);
    return MMDB_GetString(ip, path, dest, size);
}

/**
 * Gets the city name (defaults to MMDB_LANG_ENGLISH).
 */
stock bool:MMDB_GetCityName(const ip[], dest[], size = sizeof(dest), const lang[] = MMDB_LANG_ENGLISH)
{
    new path[64];
    format(path, sizeof(path), "city.names.%s", lang);
    return MMDB_GetString(ip, path, dest, size);
}

/**
 * Gets the Autonomous System Number (ASN).
 */
stock bool:MMDB_GetASN(const ip[], &dest)
{
    return MMDB_GetInt(ip, "autonomous_system_number", dest);
}

/**
 * Gets the ISP or Organization name.
 */
stock bool:MMDB_GetISP(const ip[], dest[], size = sizeof(dest))
{
    if (MMDB_GetString(ip, "isp", dest, size)) return true;
    return MMDB_GetString(ip, "autonomous_system_organization", dest, size);
}
