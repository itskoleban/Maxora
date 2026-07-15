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
// Helpers for common database fields (City, ASN, ISP)
// ----------------------------------------------------

/**
 * Gets the ISO 2-letter country code.
 */
native bool:MMDB_GetCountryCode(const ip[], dest[], size = sizeof(dest));

/**
 * Gets the English country name.
 */
native bool:MMDB_GetCountryName(const ip[], dest[], size = sizeof(dest));

/**
 * Gets the English city name.
 */
native bool:MMDB_GetCityName(const ip[], dest[], size = sizeof(dest));

/**
 * Gets the Autonomous System Number (ASN).
 */
native bool:MMDB_GetASN(const ip[], &dest);

/**
 * Gets the ISP or Organization name.
 */
native bool:MMDB_GetISP(const ip[], dest[], size = sizeof(dest));
