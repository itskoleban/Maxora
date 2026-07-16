#include <iostream>
#include <string>
#include <vector>
#include <maxminddb.h>

const char* const* PreparePath(const std::string& path, std::vector<const char*>& ptrs, std::string& buffer)
{
    buffer = path;
    ptrs.clear();
    if (buffer.empty()) {
        ptrs.push_back(nullptr);
        return ptrs.data();
    }
    char delimiter = '.';
    size_t start_idx = 0;
    if (buffer[0] == '/') {
        delimiter = '/';
        start_idx = 1;
    }
    ptrs.push_back(buffer.data() + start_idx);
    for (size_t i = start_idx; i < buffer.size(); ++i) {
        if (buffer[i] == delimiter) {
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
    
    MMDB_s mmdb;
    int status = MMDB_open("GeoLite2-Country.mmdb", MMDB_MODE_MMAP, &mmdb);
    if (status != MMDB_SUCCESS) {
        std::cerr << "Error opening database: " << MMDB_strerror(status) << "\n";
        return 1;
    }

    std::cout << "Database loaded successfully.\n";

    const char* ip = "8.8.8.8";
    int gai_err, mmdb_err;
    MMDB_lookup_result_s result = MMDB_lookup_string(&mmdb, ip, &gai_err, &mmdb_err);

    if (gai_err != 0) {
        std::cerr << "Error looking up IP: " << gai_strerror(gai_err) << "\n";
        return 1;
    }
    if (mmdb_err != 0) {
        std::cerr << "MMDB Error: " << MMDB_strerror(mmdb_err) << "\n";
        return 1;
    }

    if (result.found_entry) {
        std::cout << "IP " << ip << " found in database!\n";
        
        // Let's test the path resolution with languages
        std::vector<std::string> test_paths = {
            "country.names.en",
            "country.names.es",
            "country.names.zh-CN",
            "country.iso_code"
        };

        for (const auto& path : test_paths) {
            std::vector<const char*> ptrs;
            std::string buffer;
            const char* const* path_array = PreparePath(path, ptrs, buffer);

            MMDB_entry_data_s entry_data;
            int aget_status = MMDB_aget_value(&result.entry, &entry_data, path_array);
            
            if (aget_status == MMDB_SUCCESS && entry_data.has_data) {
                if (entry_data.type == MMDB_DATA_TYPE_UTF8_STRING) {
                    std::string value(entry_data.utf8_string, entry_data.data_size);
                    std::cout << "Path [" << path << "] -> " << value << "\n";
                } else {
                    std::cout << "Path [" << path << "] -> Data type: " << entry_data.type << " (Not a string)\n";
                }
            } else {
                std::cout << "Path [" << path << "] -> Field not found.\n";
            }
        }
    } else {
        std::cout << "IP " << ip << " not found in database.\n";
    }

    MMDB_close(&mmdb);
    return 0;
}
