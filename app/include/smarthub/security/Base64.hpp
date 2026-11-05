/**
 * Base64 encoding shared by the security classes
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace smarthub::security {

inline std::string base64Encode(const uint8_t* data, size_t len) {
    static const char kChars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string result;
    result.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<uint32_t>(data[i + 2]);

        result.push_back(kChars[(n >> 18) & 0x3F]);
        result.push_back(kChars[(n >> 12) & 0x3F]);
        result.push_back(i + 1 < len ? kChars[(n >> 6) & 0x3F] : '=');
        result.push_back(i + 2 < len ? kChars[n & 0x3F] : '=');
    }

    return result;
}

// Stops at the first '=' and skips characters outside the alphabet
inline std::vector<uint8_t> base64Decode(const std::string& encoded) {
    const auto sextet = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };

    std::vector<uint8_t> result;
    result.reserve((encoded.size() * 3) / 4);

    uint32_t val = 0;  // Unsigned: old bits are shifted out of the top
    int bits = 0;
    for (char c : encoded) {
        if (c == '=') break;
        const int value = sextet(c);
        if (value < 0) continue;

        val = (val << 6) | static_cast<uint32_t>(value);
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            result.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
        }
    }

    return result;
}

}  // namespace smarthub::security
