#include "MaxoraComponent.hpp"
#include <cstring>
#include <vector>
#include <string>

// Helper to get string from AMX
static bool GetAmxString(AMX* amx, cell amx_addr, std::string& out)
{
	IPawnScript* script = MaxoraComponent::GetPawnComponent()->getScript(amx);
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
	script->StrLen(addr, &len);
	if (len == 0)
	{
		out.clear();
		return true;
	}

	out.resize(len + 1);
	script->GetString(out.data(), addr, false, len + 1);
	out.pop_back(); // Remove the null terminator to keep std::string size accurate
	return true;
}

static bool SetAmxString(AMX* amx, cell amx_addr, const char* str, int size)
{
	IPawnScript* script = MaxoraComponent::GetPawnComponent()->getScript(amx);
	if (!script)
	{
		return false;
	}

	cell* addr;
	if (script->GetAddr(amx_addr, &addr) != 0)
	{
		return false;
	}

	script->SetString(addr, StringView(str), false, false, size);
	return true;
}

static bool SetAmxCell(AMX* amx, cell amx_addr, cell value)
{
	IPawnScript* script = MaxoraComponent::GetPawnComponent()->getScript(amx);
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

	tls_path_ptrs.push_back(tls_path_buffer.data());
	for (size_t i = 0; i < tls_path_buffer.size(); ++i)
	{
		if (tls_path_buffer[i] == '.')
		{
			tls_path_buffer[i] = '\0';
			tls_path_ptrs.push_back(&tls_path_buffer[i + 1]);
		}
	}
	tls_path_ptrs.push_back(nullptr);
	return tls_path_ptrs.data();
}

static bool GetValueFromDB(const char* ip, const char* const* path_ptrs,
						   MMDB_entry_data_s* entry_data)
{
	if (!MaxoraComponent::IsLoaded())
	{
		MaxoraComponent::SetLastError("Database not loaded.");
		return false;
	}

	int gai_err, mmdb_err;
	MMDB_lookup_result_s result =
		MMDB_lookup_string(MaxoraComponent::GetDB(), ip, &gai_err, &mmdb_err);

	if (gai_err != 0)
	{
		MaxoraComponent::SetLastError(gai_strerror(gai_err));
		return false;
	}

	if (mmdb_err != 0)
	{
		MaxoraComponent::SetLastError(MMDB_strerror(mmdb_err));
		return false;
	}

	if (!result.found_entry)
	{
		MaxoraComponent::SetLastError("IP not found.");
		return false;
	}

	int status = MMDB_aget_value(&result.entry, entry_data, path_ptrs);
	if (status != 0)
	{
		MaxoraComponent::SetLastError(MMDB_strerror(status));
		return false;
	}

	if (!entry_data->has_data)
	{
		MaxoraComponent::SetLastError("Field not found.");
		return false;
	}

	return true;
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
	MaxoraComponent::SetLastError("");
	if (params[0] < 1 * sizeof(cell))
		return 0;
	std::string filename;
	if (!GetAmxString(amx, params[1], filename))
		return 0;
	return MaxoraComponent::LoadDB(filename.c_str()) ? 1 : 0;
}

cell AMX_NATIVE_CALL n_MMDB_Unload(AMX* amx, const cell* params)
{
	MaxoraComponent::UnloadDB();
	return 1;
}

cell AMX_NATIVE_CALL n_MMDB_IsLoaded(AMX* amx, const cell* params)
{
	return MaxoraComponent::IsLoaded() ? 1 : 0;
}

cell AMX_NATIVE_CALL n_MMDB_GetLastError(AMX* amx, const cell* params)
{
	if (params[0] < 2 * sizeof(cell))
		return 0;
	const std::string& err = MaxoraComponent::GetLastError();
	int size = params[2];
	if (size <= 0)
		return 0;
	if (err.empty())
	{
		if (!SetAmxString(amx, params[1], "", size))
			return 0;
		return 0;
	}
	if (!SetAmxString(amx, params[1], err.c_str(), size))
		return 0;
	return 1;
}

cell AMX_NATIVE_CALL n_MMDB_GetString(AMX* amx, const cell* params)
{
	MaxoraComponent::SetLastError("");
	if (params[0] < 4 * sizeof(cell))
		return 0;
	std::string ip, path;
	if (!GetAmxString(amx, params[1], ip) || !GetAmxString(amx, params[2], path))
		return 0;

	int size = params[4];
	if (size <= 0)
		return 0;

	MMDB_entry_data_s entry;
	if (!GetValueFromDB(ip.c_str(), PreparePath(path), &entry))
		return 0;

	if (entry.type != MMDB_DATA_TYPE_UTF8_STRING)
	{
		MaxoraComponent::SetLastError("Field is not a string.");
		return 0;
	}

	std::string value(entry.utf8_string, entry.data_size);
	if (!SetAmxString(amx, params[3], value.c_str(), size))
	{
		MaxoraComponent::SetLastError("Invalid AMX address for destination buffer.");
		return 0;
	}
	return 1;
}

cell AMX_NATIVE_CALL n_MMDB_GetInt(AMX* amx, const cell* params)
{
	MaxoraComponent::SetLastError("");
	if (params[0] < 3 * sizeof(cell))
		return 0;
	std::string ip, path;
	if (!GetAmxString(amx, params[1], ip) || !GetAmxString(amx, params[2], path))
		return 0;

	MMDB_entry_data_s entry;
	if (!GetValueFromDB(ip.c_str(), PreparePath(path), &entry))
		return 0;

	cell result;
	switch (entry.type)
	{
	case MMDB_DATA_TYPE_UINT16:
		result = entry.uint16;
		break;
	case MMDB_DATA_TYPE_UINT32:
		result = entry.uint32;
		break;
	case MMDB_DATA_TYPE_INT32:
		result = entry.int32;
		break;
	case MMDB_DATA_TYPE_UINT64:
		result = static_cast<cell>(entry.uint64);
		break;
	default:
		MaxoraComponent::SetLastError("Field is not an integer.");
		return 0;
	}

	if (!SetAmxCell(amx, params[3], result))
	{
		MaxoraComponent::SetLastError("Invalid AMX address for destination reference.");
		return 0;
	}
	return 1;
}

cell AMX_NATIVE_CALL n_MMDB_GetFloat(AMX* amx, const cell* params)
{
	MaxoraComponent::SetLastError("");
	if (params[0] < 3 * sizeof(cell))
		return 0;
	std::string ip, path;
	if (!GetAmxString(amx, params[1], ip) || !GetAmxString(amx, params[2], path))
		return 0;

	MMDB_entry_data_s entry;
	if (!GetValueFromDB(ip.c_str(), PreparePath(path), &entry))
		return 0;

	float val = 0.0f;
	if (entry.type == MMDB_DATA_TYPE_FLOAT)
		val = entry.float_value;
	else if (entry.type == MMDB_DATA_TYPE_DOUBLE)
		val = static_cast<float>(entry.double_value);
	else
	{
		MaxoraComponent::SetLastError("Field is not a float.");
		return 0;
	}

	if (!SetAmxCell(amx, params[3], FloatToCell(val)))
	{
		MaxoraComponent::SetLastError("Invalid AMX address for destination reference.");
		return 0;
	}
	return 1;
}

cell AMX_NATIVE_CALL n_MMDB_GetBool(AMX* amx, const cell* params)
{
	MaxoraComponent::SetLastError("");
	if (params[0] < 2 * sizeof(cell))
		return 0;
	std::string ip, path;
	if (!GetAmxString(amx, params[1], ip) || !GetAmxString(amx, params[2], path))
		return 0;

	MMDB_entry_data_s entry;
	if (!GetValueFromDB(ip.c_str(), PreparePath(path), &entry))
		return 0;

	if (entry.type != MMDB_DATA_TYPE_BOOLEAN)
	{
		MaxoraComponent::SetLastError("Field is not a boolean.");
		return 0;
	}
	return entry.boolean ? 1 : 0;
}

cell AMX_NATIVE_CALL n_MMDB_HasField(AMX* amx, const cell* params)
{
	MaxoraComponent::SetLastError("");
	if (params[0] < 2 * sizeof(cell))
		return 0;
	std::string ip, path;
	if (!GetAmxString(amx, params[1], ip) || !GetAmxString(amx, params[2], path))
		return 0;

	MMDB_entry_data_s entry;
	return GetValueFromDB(ip.c_str(), PreparePath(path), &entry) ? 1 : 0;
}

cell AMX_NATIVE_CALL n_MMDB_GetNetmask(AMX* amx, const cell* params)
{
	MaxoraComponent::SetLastError("");
	if (params[0] < 2 * sizeof(cell))
		return 0;
	std::string ip;
	if (!GetAmxString(amx, params[1], ip))
		return 0;

	if (!MaxoraComponent::IsLoaded())
	{
		MaxoraComponent::SetLastError("Database not loaded.");
		return 0;
	}
	int gai_err, mmdb_err;
	MMDB_lookup_result_s result =
		MMDB_lookup_string(MaxoraComponent::GetDB(), ip.c_str(), &gai_err, &mmdb_err);

	if (gai_err != 0)
	{
		MaxoraComponent::SetLastError(gai_strerror(gai_err));
		return 0;
	}
	if (mmdb_err != 0)
	{
		MaxoraComponent::SetLastError(MMDB_strerror(mmdb_err));
		return 0;
	}
	if (!result.found_entry)
	{
		MaxoraComponent::SetLastError("IP not found.");
		return 0;
	}

	if (!SetAmxCell(amx, params[2], result.netmask))
	{
		MaxoraComponent::SetLastError("Invalid AMX address for destination reference.");
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
	if (!GetValueFromDB(ip.c_str(), path, &entry))
		return false;
	if (entry.type != MMDB_DATA_TYPE_UTF8_STRING)
	{
		MaxoraComponent::SetLastError("Field is not a string.");
		return false;
	}

	std::string value(entry.utf8_string, entry.data_size);
	if (!SetAmxString(amx, params[2], value.c_str(), size))
	{
		MaxoraComponent::SetLastError("Invalid AMX address for destination buffer.");
		return false;
	}
	return true;
}

cell AMX_NATIVE_CALL n_MMDB_GetCountryCode(AMX* amx, const cell* params)
{
	MaxoraComponent::SetLastError("");
	const char* path[] = {"country", "iso_code", nullptr};
	return GetStringHelper(amx, params, path) ? 1 : 0;
}

cell AMX_NATIVE_CALL n_MMDB_GetCountryName(AMX* amx, const cell* params)
{
	MaxoraComponent::SetLastError("");
	const char* path[] = {"country", "names", "en", nullptr};
	return GetStringHelper(amx, params, path) ? 1 : 0;
}

cell AMX_NATIVE_CALL n_MMDB_GetCityName(AMX* amx, const cell* params)
{
	MaxoraComponent::SetLastError("");
	const char* path[] = {"city", "names", "en", nullptr};
	return GetStringHelper(amx, params, path) ? 1 : 0;
}

cell AMX_NATIVE_CALL n_MMDB_GetASN(AMX* amx, const cell* params)
{
	MaxoraComponent::SetLastError("");
	if (params[0] < 2 * sizeof(cell))
		return 0;
	std::string ip;
	if (!GetAmxString(amx, params[1], ip))
		return 0;

	const char* path[] = {"autonomous_system_number", nullptr};
	MMDB_entry_data_s entry;
	if (!GetValueFromDB(ip.c_str(), path, &entry))
		return 0;

	cell result;
	if (entry.type == MMDB_DATA_TYPE_UINT32)
		result = entry.uint32;
	else if (entry.type == MMDB_DATA_TYPE_INT32)
		result = entry.int32;
	else
	{
		MaxoraComponent::SetLastError("Field is not an integer.");
		return 0;
	}

	if (!SetAmxCell(amx, params[2], result))
	{
		MaxoraComponent::SetLastError("Invalid AMX address for destination reference.");
		return 0;
	}
	return 1;
}

cell AMX_NATIVE_CALL n_MMDB_GetISP(AMX* amx, const cell* params)
{
	MaxoraComponent::SetLastError("");
	const char* path1[] = {"isp", nullptr};
	if (GetStringHelper(amx, params, path1))
		return 1;

	MaxoraComponent::SetLastError(""); // clear error before fallback
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
