#include "third_party/doctest.h"

#include "core/checkpoint.hpp"

TEST_CASE("sanitizeName matches Checkpoint") {
    CHECK(core::sanitizeName("Kid Icarus: Uprising") == "Kid Icarus  Uprising");
    CHECK(core::sanitizeName("Pokémon Moon") == "Pokémon Moon");
    CHECK(core::sanitizeName("Wait...") == "Wait");
    CHECK(core::sanitizeName("A/B\\C?*\"<>|!,") == "A B C");
}

TEST_CASE("titleFolderName") {
    CHECK(core::titleFolderName(0x0004000000175E00ULL, "Pokémon Moon") == "0x0175E Pokémon Moon");
    CHECK(core::titleFolderName(0x00040000000ABC00ULL, "X") == "0x00ABC X");
}

TEST_CASE("displayNameFromFolder") {
    CHECK(core::displayNameFromFolder("0x0175E Pokémon Moon") == "Pokémon Moon");
    CHECK(core::displayNameFromFolder("weird") == "weird");
}

TEST_CASE("versionName and uniqueName") {
    CHECK(core::versionName({2026, 11, 12, 19, 40, 5}) == "20261112-194005");
    CHECK(core::uniqueName("v", {}) == "v");
    CHECK(core::uniqueName("v", {"v", "v-2"}) == "v-3");
}

TEST_CASE("extdataIdFor") {
    CHECK(core::extdataIdFor(0x0004000000175E00ULL) == 0x1648);
    CHECK(core::extdataIdFor(0x0004000000179800ULL) == 0x1794);
    CHECK(core::extdataIdFor(0x0004000000086300ULL) == 0x0863);
}

TEST_CASE("isSystemExcluded") {
    CHECK(core::isSystemExcluded(0x0004000E00175E00ULL));
    CHECK(core::isSystemExcluded(0x0004000000008602ULL));
    CHECK(core::isSystemExcluded(0x0004800F484E4741ULL));
    CHECK_FALSE(core::isSystemExcluded(0x0004000000175E00ULL));
}
