#pragma once

#include <expected>
#include <string_view>
#include <vector>

#include "core/dav.hpp"
#include "core/error.hpp"

namespace core {

// Parses a PROPFIND Depth: 1 multistatus response. Elements are matched by
// local name whatever their namespace prefix. Each href is reduced to a plain
// path (scheme and host dropped, percent-decoded, basePath such as
// "/dav/Koofr" removed, trailing '/' removed); the entry equal to requestPath
// is the folder itself and is left out. Malformed XML → Error{Protocol}.
std::expected<std::vector<DavEntry>, Error> parsePropfind(std::string_view xml, std::string_view basePath,
                                                          std::string_view requestPath);

}  // namespace core
