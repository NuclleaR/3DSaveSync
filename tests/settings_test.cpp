#include "third_party/doctest.h"

#include "core/settings.hpp"

TEST_CASE("settings round trip") {
    core::Settings s{"me@example.com", "pa=ss word", "/Backups/3ds"};
    auto back = core::parseSettings(core::serializeSettings(s));
    REQUIRE(back);
    CHECK(back->user == "me@example.com");
    CHECK(back->password == "pa=ss word");
    CHECK(back->root == "/Backups/3ds");
}

TEST_CASE("settings defaults and rejects") {
    CHECK(core::parseSettings("user=a\npassword=b\n")->root == "/3DSaveSync");
    CHECK_FALSE(core::parseSettings("user=a\n"));
    CHECK_FALSE(core::parseSettings(""));
}

TEST_CASE("normalizeRoot") {
    CHECK(core::normalizeRoot("3DSaveSync/") == "/3DSaveSync");
    CHECK(core::normalizeRoot("/a/b//") == "/a/b");
    CHECK(core::normalizeRoot("") == "/3DSaveSync");
    CHECK(core::normalizeRoot("/") == "/3DSaveSync");
}
