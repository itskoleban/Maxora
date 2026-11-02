#include "core.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* bytes, std::size_t size) {
    const std::string_view input(reinterpret_cast<const char*>(bytes), size);
    maxora::IpAddress ip;
    (void)maxora::IpAddress::parse(input, ip);
    (void)maxora::valid_utf8(input);
    maxora::Path path;
    if (path.parse(input) == maxora::Status::Ok) {
        // Touch every returned token, checking terminators under ASan.
        for (const char* part : path.parts) {
            if (!part) break;
            (void)maxora::valid_utf8(std::string_view(part));
        }
    }
    return 0;
}
