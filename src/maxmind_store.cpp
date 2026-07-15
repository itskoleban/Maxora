/**
 * @file maxmind_store.cpp
 * @brief Implementation of the MaxMind storage engine
 */

#include "maxmind_store.hpp"

namespace maxora
{
	MMDB_s MaxmindStore::mmdb_;
	bool MaxmindStore::isLoaded_ = false;
	std::string MaxmindStore::lastError_;

	bool MaxmindStore::LoadDB(const char* filename)
	{
		MMDB_s temp_mmdb;
		int status = MMDB_open(filename, MMDB_MODE_MMAP, &temp_mmdb);
		if (status != MMDB_SUCCESS)
		{
			lastError_ = MMDB_strerror(status);
			return false;
		}

		if (isLoaded_)
		{
			MMDB_close(&mmdb_);
		}

		mmdb_ = temp_mmdb;
		isLoaded_ = true;
		lastError_.clear();
		return true;
	}

	void MaxmindStore::UnloadDB()
	{
		if (isLoaded_)
		{
			MMDB_close(&mmdb_);
			isLoaded_ = false;
		}
	}

	bool MaxmindStore::IsLoaded()
	{
		return isLoaded_;
	}

	MMDB_s* MaxmindStore::GetDB()
	{
		return &mmdb_;
	}

	const std::string& MaxmindStore::GetLastError()
	{
		return lastError_;
	}

	void MaxmindStore::SetLastError(const std::string& err)
	{
		lastError_ = err;
	}
} // namespace maxora
