#include "platform/sha256.hpp"

#include <memory>

#include "core/hex.hpp"

namespace platform {

Sha256Hasher::Sha256Hasher() {
    mbedtls_sha256_init(&ctx_);
    mbedtls_sha256_starts_ret(&ctx_, 0);  // 0: SHA-256, not SHA-224
}

Sha256Hasher::~Sha256Hasher() {
    mbedtls_sha256_free(&ctx_);
}

void Sha256Hasher::update(const char* data, size_t len) {
    mbedtls_sha256_update_ret(&ctx_, reinterpret_cast<const unsigned char*>(data), len);
}

std::string Sha256Hasher::hexDigest() {
    unsigned char digest[32];
    mbedtls_sha256_finish_ret(&ctx_, digest);
    return core::toHex(digest, sizeof(digest));
}

core::HasherFactory sha256Factory() {
    return [] { return std::make_unique<Sha256Hasher>(); };
}

}  // namespace platform
