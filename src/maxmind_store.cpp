/**
 * @file maxmind_store.cpp
 * @brief Implementation of the MaxMind storage engine
 */

#include "maxmind_store.hpp"

namespace maxora
{
	// Initialize static member variables
	MMDB_s MaxmindStore::mmdb_;
	bool MaxmindStore::isLoaded_ = false;
	std::string MaxmindStore::lastError_;

	bool MaxmindStore::LoadDB(const char* filename)
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
			return false;
		}

		// If a database was already loaded previously, we must close it before overwriting
		// the global struct to prevent memory leaks and file descriptor leaks.
		if (isLoaded_)
		{
			MMDB_close(&mmdb_);
		}

		// Assign the new database to the global state and clear any previous errors.
		mmdb_ = temp_mmdb;
		isLoaded_ = true;
		lastError_.clear();
		return true;
	}

	void MaxmindStore::UnloadDB()
	{
		// Only attempt to close if a database is currently loaded to avoid double-freeing.
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
