#pragma once

#include <cstddef>
#include <string>

namespace core {

// Lowercase hex, two digits per byte (the manifest's SHA-256 format).
std::string toHex(const unsigned char* data, size_t len);

}  // namespace core
