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

extern IPawnComponent* gPawnComponent;

namespace maxora
{

	// Helper to get string from AMX
	static bool GetAmxString(AMX* amx, cell amx_addr, std::string& out)
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

		if (len == 0)
		{
			out.clear();
			return true;
		}

		out.resize(len + 1);
		if (script->GetString(out.data(), addr, false, len + 1) != 0)
		{
			return false;
		}

		out.pop_back(); // Remove the null terminator to keep std::string size accurate
		return true;
	}

	static bool SetAmxString(AMX* amx, cell amx_addr, const char* str, int size)
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

		if (script->SetString(addr, StringView(str), false, false, size) != 0)
		{
			return false;
		}

		return true;
	}

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

	// Optimization: Replaces '.' with '\0' in a mutable buffer and populates a static vector of
	// pointers
	static thread_local std::string tls_path_buffer;
	static thread_local std::vector<const char*> tls_path_ptrs;

	static const char* const* PreparePath(const std::string& path)
	{
		tls_path_buffer = path;
		tls_path_ptrs.clear();

		if (tls_path_buffer.empty())
		{
			tls_path_ptrs.push_back(nullptr);
			return tls_path_ptrs.data();
		}

		char delimiter = '.';
		size_t start_idx = 0;
		if (tls_path_buffer[0] == '/')
		{
			delimiter = '/';
			start_idx = 1;
		}

		tls_path_ptrs.push_back(tls_path_buffer.data() + start_idx);
		for (size_t i = start_idx; i < tls_path_buffer.size(); ++i)
		{
			if (tls_path_buffer[i] == delimiter)
			{
				tls_path_buffer[i] = '\0';
				tls_path_ptrs.push_back(&tls_path_buffer[i + 1]);
			}
		}
		tls_path_ptrs.push_back(nullptr);
		return tls_path_ptrs.data();
	}

	enum class DBResult
	{
		Success,
		FieldNotFound,
		Error
	};

	static DBResult GetValueFromDB(const char* ip, const char* const* path_ptrs,
								   MMDB_entry_data_s* entry_data)
	{
		if (!MaxmindStore::IsLoaded())
		{
			MaxmindStore::SetLastError("Database not loaded.");
			return DBResult::Error;
		}

		int gai_err, mmdb_err;
		MMDB_lookup_result_s result =
			MMDB_lookup_string(MaxmindStore::GetDB(), ip, &gai_err, &mmdb_err);

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
			return DBResult::Error;
		}

		int status = MMDB_aget_value(&result.entry, entry_data, path_ptrs);
		if (status != 0)
		{
			MaxmindStore::SetLastError(MMDB_strerror(status));
			return DBResult::Error;
		}

		if (!entry_data->has_data)
		{
			return DBResult::FieldNotFound;
		}

		return DBResult::Success;
	}

	// Helper for checking success in most natives that expect the field to exist
	static bool EnsureDBSuccess(DBResult res)
	{
		if (res == DBResult::FieldNotFound)
		{
			MaxmindStore::SetLastError("Field not found.");
			return false;
		}
		return res == DBResult::Success;
	}

	// Convert float to cell
	static inline cell FloatToCell(float f)
	{
		cell c;
		std::memcpy(&c, &f, sizeof(cell));
		return c;
	}

	cell AMX_NATIVE_CALL n_MMDB_Load(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		if (params[0] < 1 * sizeof(cell))
			return 0;
		std::string filename;
		if (!GetAmxString(amx, params[1], filename))
			return 0;
		return MaxmindStore::LoadDB(filename.c_str()) ? 1 : 0;
	}

	cell AMX_NATIVE_CALL n_MMDB_Unload(AMX* amx, const cell* params)
	{
		MaxmindStore::UnloadDB();
		return 1;
	}

	cell AMX_NATIVE_CALL n_MMDB_IsLoaded(AMX* amx, const cell* params)
	{
		return MaxmindStore::IsLoaded() ? 1 : 0;
	}

	cell AMX_NATIVE_CALL n_MMDB_GetLastError(AMX* amx, const cell* params)
	{
		if (params[0] < 2 * sizeof(cell))
			return 0;
		const std::string& err = MaxmindStore::GetLastError();
		int size = params[2];
		if (size <= 0)
			return 0;
		if (err.empty())
		{
			if (!SetAmxString(amx, params[1], "", size))
				return 0;
			return 1;
		}
		if (!SetAmxString(amx, params[1], err.c_str(), size))
			return 0;
		return 1;
	}

	cell AMX_NATIVE_CALL n_MMDB_GetString(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		if (params[0] < 4 * sizeof(cell))
			return 0;
		std::string ip, path;
		if (!GetAmxString(amx, params[1], ip) || !GetAmxString(amx, params[2], path))
			return 0;

		int size = params[4];
		if (size <= 0)
			return 0;

		MMDB_entry_data_s entry;
		if (!EnsureDBSuccess(GetValueFromDB(ip.c_str(), PreparePath(path), &entry)))
			return 0;

		if (entry.type != MMDB_DATA_TYPE_UTF8_STRING)
		{
			MaxmindStore::SetLastError("Field is not a string.");
			return 0;
		}

		std::string value(entry.utf8_string, entry.data_size);
		if (!SetAmxString(amx, params[3], value.c_str(), size))
		{
			MaxmindStore::SetLastError("Invalid AMX address for destination buffer.");
			return 0;
		}
		return 1;
	}

	cell AMX_NATIVE_CALL n_MMDB_GetInt(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		if (params[0] < 3 * sizeof(cell))
			return 0;
		std::string ip, path;
		if (!GetAmxString(amx, params[1], ip) || !GetAmxString(amx, params[2], path))
			return 0;

		MMDB_entry_data_s entry;
		if (!EnsureDBSuccess(GetValueFromDB(ip.c_str(), PreparePath(path), &entry)))
			return 0;

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
				return 0;
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
				return 0;
			}
			result = static_cast<cell>(entry.uint64);
			break;
		default:
			MaxmindStore::SetLastError("Field is not an integer.");
			return 0;
		}

		if (!SetAmxCell(amx, params[3], result))
		{
			MaxmindStore::SetLastError("Invalid AMX address for destination reference.");
			return 0;
		}
		return 1;
	}

	cell AMX_NATIVE_CALL n_MMDB_GetFloat(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		if (params[0] < 3 * sizeof(cell))
			return 0;
		std::string ip, path;
		if (!GetAmxString(amx, params[1], ip) || !GetAmxString(amx, params[2], path))
			return 0;

		MMDB_entry_data_s entry;
		if (!EnsureDBSuccess(GetValueFromDB(ip.c_str(), PreparePath(path), &entry)))
			return 0;

		float val = 0.0f;
		if (entry.type == MMDB_DATA_TYPE_FLOAT)
			val = entry.float_value;
		else if (entry.type == MMDB_DATA_TYPE_DOUBLE)
			val = static_cast<float>(entry.double_value);
		else
		{
			MaxmindStore::SetLastError("Field is not a float.");
			return 0;
		}

		if (!SetAmxCell(amx, params[3], FloatToCell(val)))
		{
			MaxmindStore::SetLastError("Invalid AMX address for destination reference.");
			return 0;
		}
		return 1;
	}

	cell AMX_NATIVE_CALL n_MMDB_GetBool(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		if (params[0] < 3 * sizeof(cell))
			return 0;
		std::string ip, path;
		if (!GetAmxString(amx, params[1], ip) || !GetAmxString(amx, params[2], path))
			return 0;

		MMDB_entry_data_s entry;
		if (!EnsureDBSuccess(GetValueFromDB(ip.c_str(), PreparePath(path), &entry)))
			return 0;

		if (entry.type != MMDB_DATA_TYPE_BOOLEAN)
		{
			MaxmindStore::SetLastError("Field is not a boolean.");
			return 0;
		}

		if (!SetAmxCell(amx, params[3], entry.boolean ? 1 : 0))
		{
			MaxmindStore::SetLastError("Invalid AMX address for destination reference.");
			return 0;
		}
		return 1;
	}

	cell AMX_NATIVE_CALL n_MMDB_HasField(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		if (params[0] < 3 * sizeof(cell))
			return 0;
		std::string ip, path;
		if (!GetAmxString(amx, params[1], ip) || !GetAmxString(amx, params[2], path))
			return 0;

		MMDB_entry_data_s entry;
		DBResult res = GetValueFromDB(ip.c_str(), PreparePath(path), &entry);

		if (res == DBResult::Error)
		{
			return 0;
		}

		bool exists = (res == DBResult::Success);

		if (!SetAmxCell(amx, params[3], exists ? 1 : 0))
		{
			MaxmindStore::SetLastError("Invalid AMX address for destination reference.");
			return 0;
		}
		return 1;
	}

	cell AMX_NATIVE_CALL n_MMDB_GetNetmask(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		if (params[0] < 2 * sizeof(cell))
			return 0;
		std::string ip;
		if (!GetAmxString(amx, params[1], ip))
			return 0;

		if (!MaxmindStore::IsLoaded())
		{
			MaxmindStore::SetLastError("Database not loaded.");
			return 0;
		}
		int gai_err, mmdb_err;
		MMDB_lookup_result_s result =
			MMDB_lookup_string(MaxmindStore::GetDB(), ip.c_str(), &gai_err, &mmdb_err);

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

		if (!SetAmxCell(amx, params[2], result.netmask))
		{
			MaxmindStore::SetLastError("Invalid AMX address for destination reference.");
			return 0;
		}
		return 1;
	}

	static bool GetStringHelper(AMX* amx, const cell* params, const char* const* path)
	{
		if (params[0] < 3 * sizeof(cell))
			return false;
		std::string ip;
		if (!GetAmxString(amx, params[1], ip))
			return false;

		int size = params[3];
		if (size <= 0)
			return false;

		MMDB_entry_data_s entry;
		if (!EnsureDBSuccess(GetValueFromDB(ip.c_str(), path, &entry)))
			return false;

		if (entry.type != MMDB_DATA_TYPE_UTF8_STRING)
		{
			MaxmindStore::SetLastError("Field is not a string.");
			return false;
		}

		std::string value(entry.utf8_string, entry.data_size);
		if (!SetAmxString(amx, params[2], value.c_str(), size))
		{
			MaxmindStore::SetLastError("Invalid AMX address for destination buffer.");
			return false;
		}
		return true;
	}

	cell AMX_NATIVE_CALL n_MMDB_GetCountryCode(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		const char* path[] = {"country", "iso_code", nullptr};
		return GetStringHelper(amx, params, path) ? 1 : 0;
	}

	cell AMX_NATIVE_CALL n_MMDB_GetCountryName(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		const char* path[] = {"country", "names", "en", nullptr};
		return GetStringHelper(amx, params, path) ? 1 : 0;
	}

	cell AMX_NATIVE_CALL n_MMDB_GetCityName(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		const char* path[] = {"city", "names", "en", nullptr};
		return GetStringHelper(amx, params, path) ? 1 : 0;
	}

	cell AMX_NATIVE_CALL n_MMDB_GetASN(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		if (params[0] < 2 * sizeof(cell))
			return 0;
		std::string ip;
		if (!GetAmxString(amx, params[1], ip))
			return 0;

		const char* path[] = {"autonomous_system_number", nullptr};
		MMDB_entry_data_s entry;
		if (!EnsureDBSuccess(GetValueFromDB(ip.c_str(), path, &entry)))
			return 0;

		cell result;
		if (entry.type == MMDB_DATA_TYPE_UINT32)
		{
			if (entry.uint32 > static_cast<uint32_t>(std::numeric_limits<cell>::max()))
			{
				MaxmindStore::SetLastError("Integer overflow: value exceeds cell bounds.");
				return 0;
			}
			result = static_cast<cell>(entry.uint32);
		}
		else if (entry.type == MMDB_DATA_TYPE_INT32)
			result = entry.int32;
		else if (entry.type == MMDB_DATA_TYPE_UINT64)
		{
			if (entry.uint64 > static_cast<uint64_t>(std::numeric_limits<cell>::max()))
			{
				MaxmindStore::SetLastError("Integer overflow: value exceeds cell bounds.");
				return 0;
			}
			result = static_cast<cell>(entry.uint64);
		}
		else
		{
			MaxmindStore::SetLastError("Field is not an integer.");
			return 0;
		}

		if (!SetAmxCell(amx, params[2], result))
		{
			MaxmindStore::SetLastError("Invalid AMX address for destination reference.");
			return 0;
		}
		return 1;
	}

	cell AMX_NATIVE_CALL n_MMDB_GetISP(AMX* amx, const cell* params)
	{
		MaxmindStore::SetLastError("");
		const char* path1[] = {"isp", nullptr};
		if (GetStringHelper(amx, params, path1))
			return 1;

		MaxmindStore::SetLastError(""); // clear error before fallback
		const char* path2[] = {"autonomous_system_organization", nullptr};
		return GetStringHelper(amx, params, path2) ? 1 : 0;
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
											  {"MMDB_GetCountryCode", n_MMDB_GetCountryCode},
											  {"MMDB_GetCountryName", n_MMDB_GetCountryName},
											  {"MMDB_GetCityName", n_MMDB_GetCityName},
											  {"MMDB_GetASN", n_MMDB_GetASN},
											  {"MMDB_GetISP", n_MMDB_GetISP},
											  {0, 0}};

	void RegisterNatives(IPawnScript& script)
	{
		script.Register(maxora_natives, sizeof(maxora_natives) / sizeof(AMX_NATIVE_INFO) - 1);
	}

} // namespace maxora
