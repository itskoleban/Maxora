/**
 * @file natives.cpp
 * @brief Implementation of the Pawn AMX natives for Maxora.
 */

#include "natives.hpp"
#include "maxmind_store.hpp"
#include <amx/amx.h>
#include <cstring>
#include <string>
#include <vector>
#include <limits>

#ifdef _WIN32
#include <Ws2tcpip.h>
#else
#include <netdb.h>
#endif

extern IPawnComponent* gPawnComponent;

namespace maxora
{
	/**
	 * @brief Helper to safely extract a string from the AMX machine into a fixed C-string buffer.
	 * @param amx The AMX instance.
	 * @param amx_addr The AMX memory address containing the string.
	 * @param out The destination C-string buffer.
	 * @param max_len The maximum capacity of the destination buffer.
	 * @return True if extraction was successful, false if memory errors occurred or if string was
	 * too long.
	 */
	static bool GetAmxString(AMX* amx, cell amx_addr, char* out, int max_len)
	{
		if (!gPawnComponent)
		{
			return false;
		}

		IPawnScript* script = gPawnComponent->getScript(amx);
		if (!script)
		{
			return false;
		}

		cell* addr;
		if (script->GetAddr(amx_addr, &addr) != 0)
		{
			return false;
		}

		int len;
		if (script->StrLen(addr, &len) != 0)
		{
			return false;
		}

		if (len >= max_len)
		{
			return false; // Fail gracefully if the AMX string exceeds our stack buffer capacity.
		}

		if (len == 0)
		{
			out[0] = '\0';
			return true;
		}

		if (script->GetString(out, addr, false, len + 1) != 0)
		{
			return false;
		}

		return true;
	}

	/**
	 * @brief Helper to write a C-string back into the AMX machine memory.
	 * @param amx The AMX instance.
	 * @param amx_addr The AMX memory address pointing to the destination array.
	 * @param str The source C-string to copy (does not need to be null-terminated).
	 * @param data_size The actual length of the source string in bytes.
	 * @param max_size The maximum capacity of the AMX destination array.
	 * @return True on success, false on memory error.
	 */
	static bool SetAmxString(AMX* amx, cell amx_addr, const char* str, uint32_t data_size,
							 int max_size)
	{
		if (!gPawnComponent)
		{
			return false;
		}

		IPawnScript* script = gPawnComponent->getScript(amx);
		if (!script)
		{
			return false;
		}

		cell* addr;
		if (script->GetAddr(amx_addr, &addr) != 0)
		{
			return false;
		}

		if (max_size <= 0)
		{
			MaxmindStore::SetLastError("Invalid size parameter.");
			return false;
		}

		int len = static_cast<int>(data_size);
		if (len >= max_size)
		{
			len = max_size - 1;
		}

		// Use open.mp's StringView which safely accepts a pointer and length without requiring
		// null-termination.
		if (script->SetString(addr, StringView(str, len), false, false, max_size) != 0)
		{
			return false;
		}

		return true;
	}

	/**
	 * @brief Helper to write a single integer/float cell back into the AMX machine by reference.
	 * @param amx The AMX instance.
	 * @param amx_addr The AMX memory address (reference pointer).
	 * @param value The value to set.
	 * @return True on success, false on memory error.
	 */
	static bool SetAmxCell(AMX* amx, cell amx_addr, cell value)
	{
		if (!gPawnComponent)
		{
			return false;
		}

		IPawnScript* script = gPawnComponent->getScript(amx);
		if (!script)
		{
			return false;
		}

		cell* addr;
		if (script->GetAddr(amx_addr, &addr) != 0)
		{
			return false;
		}
		*addr = value;
		return true;
	}

	// Optimization: We use a thread-local static array of pointers to avoid any dynamic memory
	// allocations (std::vector) during the path parsing phase. Since Pawn scripts in open.mp run in
	// a specific thread, thread_local provides safety without mutex locking overhead if
	// multi-threading is ever introduced.
	static thread_local const char* tls_path_ptrs[128];

	/**
	 * @brief Parses a dot-separated (or slash-separated) path string into an array of pointers.
	 *
	 * This function modifies the input `path` buffer in-place by replacing delimiters with null
	 * terminators (`\0`), achieving highly efficient in-place parsing.
	 *
	 * @param path The mutable path string (e.g., "country.names.en" becomes
	 * "country\0names\0en\0").
	 * @return A null-terminated array of pointers to each path segment, required by libmaxminddb.
	 */
	static const char* const* PreparePath(char* path)
	{
		int idx = 0;
		if (!path || path[0] == '\0')
		{
			tls_path_ptrs[0] = nullptr;
			return tls_path_ptrs;
		}

		char delimiter = '.';
		if (path[0] == '/')
		{
			delimiter = '/';
			path++;
		}

		tls_path_ptrs[idx++] = path;
		for (char* p = path; *p; ++p)
		{
			if (*p == delimiter)
			{
				*p = '\0'; // Mutate the string directly on the stack to separate nodes
				if (idx >= 127)
				{
					MaxmindStore::SetLastError("Path contains too many segments (limit 127).");
					return nullptr;
				}
				tls_path_ptrs[idx++] = p + 1;
			}
		}
		tls_path_ptrs[idx] = nullptr;
		return tls_path_ptrs;
	}

	/**
	 * @brief Represents the internal outcome of a database query.
	 */
	enum class DBResult : cell
	{
		Success = 1,
		Error = 0,
		NotFound = -1,
		InvalidHandle = -2,
		InvalidParams = -3,
		TypeMismatch = -4
	};

	/**
	 * @brief Core engine function that queries the libmaxminddb tree for a specific path.
	 * @param ip The IP address string to query.
	 * @param path_ptrs The parsed array of path segments.
	 * @param entry_data Pointer to the libmaxminddb struct that will hold the result data.
	 * @return A DBResult indicating the precise outcome of the query.
	 */
	static DBResult GetValueFromDB(int handle, const char* ip, const char* const* path_ptrs,
								   MMDB_entry_data_s* entry_data)
	{
		if (!path_ptrs)
		{
			// The error message was already set by PreparePath
			return DBResult::InvalidParams;
		}

		MMDB_s* db = MaxmindStore::GetDB(handle);
		if (!db)
		{
			MaxmindStore::SetLastError("Invalid database handle.");
			return DBResult::InvalidHandle;
		}

		int gai_err, mmdb_err;
		MMDB_lookup_result_s result = MMDB_lookup_string(db, ip, &gai_err, &mmdb_err);

		if (gai_err != 0)
		{
			MaxmindStore::SetLastError(gai_strerror(gai_err));
			return DBResult::Error;
		}

		if (mmdb_err != 0)
		{
			MaxmindStore::SetLastError(MMDB_strerror(mmdb_err));
			return DBResult::Error;
		}

		if (!result.found_entry)
		{
			MaxmindStore::SetLastError("IP not found.");
			return DBResult::NotFound;
		}

		int status = MMDB_aget_value(&result.entry, entry_data, path_ptrs);

		if (status == MMDB_LOOKUP_PATH_DOES_NOT_MATCH_DATA_ERROR ||
			(status == MMDB_SUCCESS && !entry_data->has_data))
		{
			MaxmindStore::SetLastError("Field not found.");
			return DBResult::NotFound;
		}

		if (status != MMDB_SUCCESS)
		{
			MaxmindStore::SetLastError(MMDB_strerror(status));
			return DBResult::Error;
		}

		return DBResult::Success;
	}



	/**
	 * @brief Converts a C++ float to a Pawn cell without casting the value, preserving the bit
	 * pattern.
	 */
	static inline cell FloatToCell(float f)
	{
		cell c;
		std::memcpy(&c, &f, sizeof(cell));
		return c;
	}

	/**
	 * @brief native MMDB_Load(const filename[]);
	 * Loads a database into memory.
	 */
	cell AMX_NATIVE_CALL n_MMDB_Load(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		if (params[0] < 1 * sizeof(cell))
		{
			MaxmindStore::SetLastError("Invalid number of parameters.");
			return static_cast<cell>(DBResult::InvalidParams);
		}
		char filename[256];
		if (!GetAmxString(amx, params[1], filename, sizeof(filename)))
		{
			MaxmindStore::SetLastError("Invalid AMX address or filename too long.");
			return 0;
		}
		return MaxmindStore::LoadDB(filename);
	}

	/**
	 * @brief native MMDB_Unload();
	 * Unloads the current database from memory.
	 */
	cell AMX_NATIVE_CALL n_MMDB_Unload(AMX* amx, const cell* params)
	{
		if (params[0] < 1 * sizeof(cell))
		{
			MaxmindStore::SetLastError("Invalid number of parameters.");
			return static_cast<cell>(DBResult::InvalidParams);
		}
		if (!MaxmindStore::UnloadDB(params[1]))
		{
			return static_cast<cell>(DBResult::InvalidHandle);
		}
		return static_cast<cell>(DBResult::Success);
	}

	/**
	 * @brief native MMDB_IsLoaded();
	 * Returns 1 if a database is actively loaded, 0 otherwise.
	 */
	cell AMX_NATIVE_CALL n_MMDB_IsLoaded(AMX* amx, const cell* params)
	{
		if (params[0] < 1 * sizeof(cell))
			return 0;
		return MaxmindStore::IsLoaded(params[1]) ? 1 : 0;
	}

	/**
	 * @brief native MMDB_GetLastError(dest[], size = sizeof(dest));
	 * Retrieves the last internal error message generated by the plugin.
	 */
	cell AMX_NATIVE_CALL n_MMDB_GetLastError(AMX* amx, const cell* params)
	{
		if (params[0] < 2 * sizeof(cell))
			return 0;

		const std::string& err = MaxmindStore::GetLastError();
		if (err.empty())
		{
			if (!SetAmxString(amx, params[1], "", 0, params[2]))
			{
				return 0;
			}
			return 0;
		}

		if (!SetAmxString(amx, params[1], err.c_str(), static_cast<uint32_t>(err.size()),
						  params[2]))
		{
			return 0;
		}
		return static_cast<cell>(DBResult::Success);
	}

	/**
	 * @brief native MMDB_GetString(MMDB:handle, const ip[], const path[], dest[], size = sizeof(dest));
	 * Retrieves a string value from the database.
	 */
	cell AMX_NATIVE_CALL n_MMDB_GetString(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		if (params[0] < 5 * sizeof(cell))
		{
			MaxmindStore::SetLastError("Invalid number of parameters.");
			return static_cast<cell>(DBResult::InvalidParams);
		}
		int handle = params[1];
		char ip[64];
		char path[128];
		if (!GetAmxString(amx, params[2], ip, sizeof(ip)) ||
			!GetAmxString(amx, params[3], path, sizeof(path)))
		{
			MaxmindStore::SetLastError("Invalid AMX address or string too long.");
			return static_cast<cell>(DBResult::InvalidParams);
		}

		int size = params[5];
		if (size <= 0)
		{
			MaxmindStore::SetLastError("Invalid size parameter.");
			return static_cast<cell>(DBResult::InvalidParams);
		}

		MMDB_entry_data_s entry;
		DBResult res = GetValueFromDB(handle, ip, PreparePath(path), &entry);
		if (res != DBResult::Success)
			return static_cast<cell>(res);

		if (entry.type != MMDB_DATA_TYPE_UTF8_STRING)
		{
			MaxmindStore::SetLastError("Field is not a string.");
			return static_cast<cell>(DBResult::TypeMismatch);
		}

		if (!SetAmxString(amx, params[4], entry.utf8_string, entry.data_size, size))
		{
			MaxmindStore::SetLastError("Failed to write string to AMX.");
			return static_cast<cell>(DBResult::InvalidParams);
		}
		return static_cast<cell>(DBResult::Success);
	}

	/**
	 * @brief native MMDB_GetInt(MMDB:handle, const ip[], const path[], &dest);
	 * Retrieves a 16-bit, 32-bit, or 64-bit integer from the database.
	 * Checks for overflow against the Pawn cell maximum limit.
	 */
	cell AMX_NATIVE_CALL n_MMDB_GetInt(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		if (params[0] < 4 * sizeof(cell))
		{
			MaxmindStore::SetLastError("Invalid number of parameters.");
			return static_cast<cell>(DBResult::InvalidParams);
		}
		int handle = params[1];
		char ip[64];
		char path[128];
		if (!GetAmxString(amx, params[2], ip, sizeof(ip)) ||
			!GetAmxString(amx, params[3], path, sizeof(path)))
		{
			MaxmindStore::SetLastError("Invalid AMX address or string too long.");
			return static_cast<cell>(DBResult::InvalidParams);
		}

		MMDB_entry_data_s entry;
		DBResult res = GetValueFromDB(handle, ip, PreparePath(path), &entry);
		if (res != DBResult::Success)
			return static_cast<cell>(res);

		cell result;
		switch (entry.type)
		{
		case MMDB_DATA_TYPE_UINT16:
			result = entry.uint16;
			break;
		case MMDB_DATA_TYPE_UINT32:
			if (entry.uint32 > static_cast<uint32_t>(std::numeric_limits<cell>::max()))
			{
				MaxmindStore::SetLastError("Integer overflow: value exceeds cell bounds.");
				return static_cast<cell>(DBResult::TypeMismatch);
			}
			result = static_cast<cell>(entry.uint32);
			break;
		case MMDB_DATA_TYPE_INT32:
			result = entry.int32;
			break;
		case MMDB_DATA_TYPE_UINT64:
			if (entry.uint64 > static_cast<uint64_t>(std::numeric_limits<cell>::max()))
			{
				MaxmindStore::SetLastError("Integer overflow: value exceeds cell bounds.");
				return static_cast<cell>(DBResult::TypeMismatch);
			}
			result = static_cast<cell>(entry.uint64);
			break;
		default:
			MaxmindStore::SetLastError("Field is not an integer.");
			return static_cast<cell>(DBResult::TypeMismatch);
		}

		if (!SetAmxCell(amx, params[4], result))
		{
			MaxmindStore::SetLastError("Invalid AMX address for destination reference.");
			return static_cast<cell>(DBResult::InvalidParams);
		}
		return static_cast<cell>(DBResult::Success);
	}

	/**
	 * @brief native MMDB_GetFloat(const ip[], const path[], &Float:dest);
	 * Retrieves a float or double from the database, casting it to a 32-bit Pawn float.
	 */
	cell AMX_NATIVE_CALL n_MMDB_GetFloat(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		if (params[0] < 4 * sizeof(cell))
		{
			MaxmindStore::SetLastError("Invalid number of parameters.");
			return static_cast<cell>(DBResult::InvalidParams);
		}
		int handle = params[1];
		char ip[64];
		char path[128];
		if (!GetAmxString(amx, params[2], ip, sizeof(ip)) ||
			!GetAmxString(amx, params[3], path, sizeof(path)))
		{
			MaxmindStore::SetLastError("Invalid AMX address or string too long.");
			return static_cast<cell>(DBResult::InvalidParams);
		}

		MMDB_entry_data_s entry;
		DBResult res = GetValueFromDB(handle, ip, PreparePath(path), &entry);
		if (res != DBResult::Success)
			return static_cast<cell>(res);

		float val = 0.0f;
		if (entry.type == MMDB_DATA_TYPE_FLOAT)
			val = entry.float_value;
		else if (entry.type == MMDB_DATA_TYPE_DOUBLE)
			val = static_cast<float>(entry.double_value);
		else
		{
			MaxmindStore::SetLastError("Field is not a float.");
			return static_cast<cell>(DBResult::TypeMismatch);
		}

		if (!SetAmxCell(amx, params[4], FloatToCell(val)))
		{
			MaxmindStore::SetLastError("Invalid AMX address for destination reference.");
			return static_cast<cell>(DBResult::InvalidParams);
		}
		return static_cast<cell>(DBResult::Success);
	}

	/**
	 * @brief native MMDB_GetBool(const ip[], const path[], &bool:dest);
	 * Retrieves a boolean value from the database.
	 */
	cell AMX_NATIVE_CALL n_MMDB_GetBool(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		if (params[0] < 4 * sizeof(cell))
		{
			MaxmindStore::SetLastError("Invalid number of parameters.");
			return static_cast<cell>(DBResult::InvalidParams);
		}
		int handle = params[1];
		char ip[64];
		char path[128];
		if (!GetAmxString(amx, params[2], ip, sizeof(ip)) ||
			!GetAmxString(amx, params[3], path, sizeof(path)))
		{
			MaxmindStore::SetLastError("Invalid AMX address or string too long.");
			return static_cast<cell>(DBResult::InvalidParams);
		}

		MMDB_entry_data_s entry;
		DBResult res = GetValueFromDB(handle, ip, PreparePath(path), &entry);
		if (res != DBResult::Success)
			return static_cast<cell>(res);

		if (entry.type != MMDB_DATA_TYPE_BOOLEAN)
		{
			MaxmindStore::SetLastError("Field is not a boolean.");
			return static_cast<cell>(DBResult::TypeMismatch);
		}

		if (!SetAmxCell(amx, params[4], entry.boolean ? 1 : 0))
		{
			MaxmindStore::SetLastError("Invalid AMX address for destination reference.");
			return static_cast<cell>(DBResult::InvalidParams);
		}
		return static_cast<cell>(DBResult::Success);
	}

	/**
	 * @brief native MMDB_HasField(const ip[], const path[], &bool:exists);
	 * Checks if a specific path exists within the IP's data structure without extracting it.
	 */
	cell AMX_NATIVE_CALL n_MMDB_HasField(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		if (params[0] < 4 * sizeof(cell))
		{
			MaxmindStore::SetLastError("Invalid number of parameters.");
			return static_cast<cell>(DBResult::InvalidParams);
		}
		int handle = params[1];
		char ip[64];
		char path[128];
		if (!GetAmxString(amx, params[2], ip, sizeof(ip)) ||
			!GetAmxString(amx, params[3], path, sizeof(path)))
		{
			MaxmindStore::SetLastError("Invalid AMX address or string too long.");
			return static_cast<cell>(DBResult::InvalidParams);
		}

		MMDB_entry_data_s entry;
		DBResult res = GetValueFromDB(handle, ip, PreparePath(path), &entry);
		if (res == DBResult::InvalidHandle || res == DBResult::InvalidParams) return static_cast<cell>(res);
		bool exists = (res == DBResult::Success);

		if (!SetAmxCell(amx, params[4], exists ? 1 : 0))
		{
			MaxmindStore::SetLastError("Invalid AMX address for destination reference.");
			return static_cast<cell>(DBResult::InvalidParams);
		}
		return static_cast<cell>(DBResult::Success);
	}

	/**
	 * @brief native MMDB_GetNetmask(const ip[], &dest);
	 * Retrieves the routing prefix (netmask) associated with the IP.
	 */
	cell AMX_NATIVE_CALL n_MMDB_GetNetmask(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		if (params[0] < 3 * sizeof(cell))
		{
			MaxmindStore::SetLastError("Invalid number of parameters.");
			return static_cast<cell>(DBResult::InvalidParams);
		}
		int handle = params[1];
		char ip[64];
		if (!GetAmxString(amx, params[2], ip, sizeof(ip)))
		{
			MaxmindStore::SetLastError("Invalid AMX address or string too long.");
			return static_cast<cell>(DBResult::InvalidParams);
		}

		MMDB_s* db = MaxmindStore::GetDB(handle);
		if (!db)
		{
			MaxmindStore::SetLastError("Invalid database handle.");
			return 0;
		}
		int gai_err, mmdb_err;
		MMDB_lookup_result_s result = MMDB_lookup_string(db, ip, &gai_err, &mmdb_err);

		if (gai_err != 0)
		{
			MaxmindStore::SetLastError(gai_strerror(gai_err));
			return 0;
		}
		if (mmdb_err != 0)
		{
			MaxmindStore::SetLastError(MMDB_strerror(mmdb_err));
			return 0;
		}
		if (!result.found_entry)
		{
			MaxmindStore::SetLastError("IP not found.");
			return 0;
		}

		if (!SetAmxCell(amx, params[3], result.netmask))
		{
			MaxmindStore::SetLastError("Invalid AMX address for destination reference.");
			return static_cast<cell>(DBResult::InvalidParams);
		}
		return static_cast<cell>(DBResult::Success);
	}

	const AMX_NATIVE_INFO maxora_natives[] = {{"MMDB_Load", n_MMDB_Load},
											  {"MMDB_Unload", n_MMDB_Unload},
											  {"MMDB_IsLoaded", n_MMDB_IsLoaded},
											  {"MMDB_GetString", n_MMDB_GetString},
											  {"MMDB_GetInt", n_MMDB_GetInt},
											  {"MMDB_GetFloat", n_MMDB_GetFloat},
											  {"MMDB_GetBool", n_MMDB_GetBool},
											  {"MMDB_HasField", n_MMDB_HasField},
											  {"MMDB_GetNetmask", n_MMDB_GetNetmask},
											  {"MMDB_GetLastError", n_MMDB_GetLastError},
											  {0, 0}};

	void RegisterNatives(IPawnScript& script)
	{
		script.Register(maxora_natives, sizeof(maxora_natives) / sizeof(AMX_NATIVE_INFO) - 1);
	}

} // namespace maxora
