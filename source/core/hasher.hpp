#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace core {

class Hasher {
public:
    virtual ~Hasher() = default;
    virtual void update(const char* data, size_t len) = 0;
    virtual std::string hexDigest() = 0;  // 64 lowercase hex digits for SHA-256
};

using HasherFactory = std::function<std::unique_ptr<Hasher>()>;

}  // namespace core
