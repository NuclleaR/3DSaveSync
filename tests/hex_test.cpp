#include <array>

#include "core/hex.hpp"
#include "doctest.h"

TEST_CASE("toHex writes lowercase pairs") {
    const std::array<unsigned char, 4> bytes{0x00, 0x0f, 0xa0, 0xff};
    CHECK(core::toHex(bytes.data(), bytes.size()) == "000fa0ff");
}

TEST_CASE("toHex of nothing is empty") {
    CHECK(core::toHex(nullptr, 0).empty());
}
