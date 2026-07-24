/**
 * @file test_backend.cpp
 * @brief Standalone C++ test executable for Maxora backend logic.
 *
 * This file is used to test libmaxminddb integration without needing a running SA-MP / open.mp
 * server. It compiles to a standard terminal executable (`maxora_test.exe`) and attempts to read a
 * database file to verify the behavior of the API.
 */

#include <iostream>
#include <string>
#include <vector>
#include <maxminddb.h>

#ifdef _WIN32
#include <Ws2tcpip.h>
#else
#include <netdb.h>
#endif

/**
 * @brief Parses a path string into a libmaxminddb pointer array.
 *
 * Note: This test implementation uses std::vector and std::string for simplicity.
 * The actual plugin implementation (in natives.cpp) uses a zero-allocation approach.
 */
const char* const* PreparePath(const std::string& path, std::vector<const char*>& ptrs,
							   std::string& buffer)
{
	buffer = path;
	ptrs.clear();

	if (buffer.empty())
	{
		ptrs.push_back(nullptr);
		return ptrs.data();
	}

	char delimiter = '.';
	size_t start_idx = 0;
	if (buffer[0] == '/')
	{
		delimiter = '/';
		start_idx = 1;
	}

	ptrs.push_back(buffer.data() + start_idx);
	for (size_t i = start_idx; i < buffer.size(); ++i)
	{
		if (buffer[i] == delimiter)
		{
			buffer[i] = '\0';
			ptrs.push_back(&buffer[i + 1]);
		}
	}
	ptrs.push_back(nullptr);
	return ptrs.data();
}

int main()
{
	std::cout << "--- Maxora Backend Test ---\n";

	// 1. Initialize and open the database
	MMDB_s mmdb;
	int status = MMDB_open("GeoLite2-Country.mmdb", MMDB_MODE_MMAP, &mmdb);
	if (status != MMDB_SUCCESS)
	{
		std::cerr << "Error opening database: " << MMDB_strerror(status) << "\n";
		std::cerr
			<< "Make sure 'GeoLite2-Country.mmdb' is in the same directory as the executable.\n";
		return 1;
	}

	std::cout << "Database loaded successfully.\n";

	// 2. Perform a lookup for a specific IP (Google Public DNS)
	const char* ip = "8.8.8.8";
	int gai_err, mmdb_err;
	MMDB_lookup_result_s result = MMDB_lookup_string(&mmdb, ip, &gai_err, &mmdb_err);

	// 3. Handle lookup errors
	if (gai_err != 0)
	{
		std::cerr << "Error looking up IP: " << gai_strerror(gai_err) << "\n";
		MMDB_close(&mmdb);
		return 1;
	}
	if (mmdb_err != 0)
	{
		std::cerr << "MMDB Error: " << MMDB_strerror(mmdb_err) << "\n";
		MMDB_close(&mmdb);
		return 1;
	}

	// 4. Verify if the IP exists in the database
	if (result.found_entry)
	{
		std::cout << "IP " << ip << " found in database!\n";

		// 5. Test path resolution with different languages
		std::vector<std::string> test_paths = {"country.names.en", "country.names.es",
											   "country.names.zh-CN", "country.iso_code"};

		for (const auto& path : test_paths)
		{
			std::vector<const char*> ptrs;
			std::string buffer;
			const char* const* path_array = PreparePath(path, ptrs, buffer);

			// Fetch the specific nested field using the path array
			MMDB_entry_data_s entry_data;
			int aget_status = MMDB_aget_value(&result.entry, &entry_data, path_array);

			// Check if the query succeeded and the field actually contains data
			if (aget_status == MMDB_SUCCESS && entry_data.has_data)
			{
				if (entry_data.type == MMDB_DATA_TYPE_UTF8_STRING)
				{
					// Strings in libmaxminddb are not null-terminated, so we must construct
					// std::string with data_size
					std::string value(entry_data.utf8_string, entry_data.data_size);
					std::cout << "Path [" << path << "] -> " << value << "\n";
				}
				else
				{
					std::cout << "Path [" << path << "] -> Data type: " << entry_data.type
							  << " (Not a string)\n";
				}
			}
			else
			{
				std::cout << "Path [" << path << "] -> Field not found.\n";
			}
		}
	}
	else
	{
		std::cout << "IP " << ip << " not found in database.\n";
	}

	// 6. Cleanup memory
	MMDB_close(&mmdb);
	return 0;
}
