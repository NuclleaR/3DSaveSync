#include "third_party/doctest.h"

#include "core/url.hpp"

TEST_CASE("encodePath / decodePercent round trip") {
    CHECK(core::encodePath("/3DSaveSync/saves/0x0175E Pokémon Moon") == "/3DSaveSync/saves/0x0175E%20Pok%C3%A9mon%20Moon");
    CHECK(core::encodePath("/a+b&c") == "/a%2Bb%26c");
    CHECK(core::decodePercent("Pok%C3%A9mon%20Moon") == "Pokémon Moon");
    CHECK(core::decodePercent(core::encodePath("/Kid Icarus  Uprising/x")) == "/Kid Icarus  Uprising/x");
}
