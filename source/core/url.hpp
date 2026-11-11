#pragma once

#include <string>
#include <string_view>

namespace core {

// Percent-encodes every byte except A-Z a-z 0-9 - . _ ~ and '/'.
std::string encodePath(std::string_view path);

// %XX → byte; '+' stays '+'; malformed sequences are kept as they are.
std::string decodePercent(std::string_view s);

}  // namespace core
