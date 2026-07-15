/**
 * @file maxmind_store.hpp
 * @brief Database storage engine for Maxora
 *
 * Provides static methods and global state management for the libmaxminddb connection.
 */

#pragma once

#include <maxminddb.h>
#include <string>

namespace maxora
{
	class MaxmindStore
	{
	  public:
		static bool LoadDB(const char* filename);
		static void UnloadDB();
		static bool IsLoaded();
		static MMDB_s* GetDB();
		static const std::string& GetLastError();
		static void SetLastError(const std::string& err);

	  private:
		static MMDB_s mmdb_;
		static bool isLoaded_;
		static std::string lastError_;
	};
} // namespace maxora
