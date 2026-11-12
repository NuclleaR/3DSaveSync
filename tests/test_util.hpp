#pragma once

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

// Deterministic stand-in for SHA-256 in tests: FNV-1a 64-bit as 16 hex digits,
// repeated to the 64 lowercase hex digits a manifest expects.
inline std::string fakeDigest(std::string_view data) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : data) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    char hex[17];
    std::snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(h));
    return std::string(hex) + hex + hex + hex;
}

// Reads a test fixture relative to the tests/ directory (make -C tests runs there).
inline std::string readFile(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
