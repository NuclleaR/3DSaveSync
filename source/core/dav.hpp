#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include "core/error.hpp"
#include "core/http.hpp"

namespace core {

struct DavEntry {
    std::string name;
    bool isDir;
    uint64_t size;
};

// WebDAV operations on plain absolute paths such as
// "/3DSaveSync/saves/0x0175E Pokémon Moon" (encoding is the client's job).
class Dav {
public:
    virtual ~Dav() = default;
    virtual std::expected<std::vector<DavEntry>, Error> list(const std::string& path) = 0;  // missing → NotFound
    virtual std::expected<void, Error> makeDirs(const std::string& path) = 0;  // creates each missing segment
    virtual std::expected<void, Error> put(const std::string& path, BodySource body) = 0;
    virtual std::expected<std::string, Error> getText(const std::string& path) = 0;
    virtual std::expected<void, Error> get(const std::string& path, BodySink& sink) = 0;
    virtual std::expected<void, Error> move(const std::string& from, const std::string& to) = 0;  // destination exists → Conflict
    virtual std::expected<void, Error> remove(const std::string& path) = 0;  // missing → success
};

}  // namespace core
