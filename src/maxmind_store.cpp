/**
 * @file maxmind_store.cpp
 * @brief Implementation of the MaxMind storage engine
 */

#include <unordered_map>
#include "maxmind_store.hpp"

namespace maxora
{
	// Initialize static member variables
	std::unordered_map<int, MMDB_s> MaxmindStore::databases_;
	int MaxmindStore::nextHandle_ = 1;
	std::string MaxmindStore::lastError_;

	int MaxmindStore::LoadDB(const char* filename)
	{
		MMDB_s temp_mmdb;

		// Attempt to open the database file using memory mapping (MMDB_MODE_MMAP).
		// Memory mapping is highly efficient for large databases as it allows the OS
		// to page data in and out of RAM as needed without loading the entire file.
		int status = MMDB_open(filename, MMDB_MODE_MMAP, &temp_mmdb);

		if (status != MMDB_SUCCESS)
		{
			// If opening fails, store the libmaxminddb error message and abort.
			lastError_ = MMDB_strerror(status);
			return 0; // Return invalid handle
		}

		// Assign a unique handle to the new database and store it.
		int handle = nextHandle_++;
		databases_[handle] = temp_mmdb;
		lastError_.clear();
		return handle;
	}

	bool MaxmindStore::UnloadDB(int handle)
	{
		auto it = databases_.find(handle);
		if (it != databases_.end())
		{
			MMDB_close(&it->second);
			databases_.erase(it);

			if (databases_.empty())
			{
				nextHandle_ = 1;
			}
			return true;
		}

		SetLastError("Invalid database handle.");
		return false;
	}

	void MaxmindStore::UnloadAll()
	{
		for (auto& pair : databases_)
		{
			MMDB_close(&pair.second);
		}
		databases_.clear();
		nextHandle_ = 1;
		lastError_.clear();
	}

	bool MaxmindStore::IsLoaded(int handle)
	{
		return databases_.find(handle) != databases_.end();
	}

	MMDB_s* MaxmindStore::GetDB(int handle)
	{
		auto it = databases_.find(handle);
		if (it != databases_.end())
		{
			return &it->second;
		}
		return nullptr;
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
