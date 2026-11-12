#pragma once

#include <mbedtls/sha256.h>

#include <string>

#include "core/hasher.hpp"

namespace platform {

// SHA-256 through mbedTLS 2.28.
class Sha256Hasher : public core::Hasher {
public:
    Sha256Hasher();
    ~Sha256Hasher() override;
    Sha256Hasher(const Sha256Hasher&) = delete;
    Sha256Hasher& operator=(const Sha256Hasher&) = delete;

    void update(const char* data, size_t len) override;
    std::string hexDigest() override;

private:
    mbedtls_sha256_context ctx_;
};

core::HasherFactory sha256Factory();

}  // namespace platform
