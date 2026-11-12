#include "third_party/doctest.h"

#include "core/settings.hpp"

TEST_CASE("settings round trip") {
    core::Settings s{"me@example.com", "pa=ss word", "/Backups/3ds", false};
    auto back = core::parseSettings(core::serializeSettings(s));
    REQUIRE(back);
    CHECK(back->user == "me@example.com");
    CHECK(back->password == "pa=ss word");
    CHECK(back->root == "/Backups/3ds");
    CHECK_FALSE(back->autoBackup);
}

TEST_CASE("settings defaults and rejects") {
    auto old = core::parseSettings("user=a\npassword=b\n");
    REQUIRE(old);
    CHECK(old->root == "/3DSaveSync");
    CHECK(old->autoBackup);  // files written before the option existed back up on start
    CHECK_FALSE(core::parseSettings("user=a\n"));
    CHECK_FALSE(core::parseSettings(""));
    CHECK_FALSE(core::parseSettings("user=\npassword=\nroot=/X\n"));  // signed out
}

TEST_CASE("auto backup is off only for autobackup=0") {
    CHECK_FALSE(core::parseSettings("user=a\npassword=b\nautobackup=0\n")->autoBackup);
    CHECK(core::parseSettings("user=a\npassword=b\nautobackup=1\n")->autoBackup);
}

TEST_CASE("signed-out settings keep the cloud folder and auto backup") {
    core::Settings kept{"", "", "/X", false};
    const std::string text = core::serializeSettings(kept);
    CHECK_FALSE(core::parseSettings(text));
    const core::Settings back = core::parseKept(text);
    CHECK(back.root == "/X");
    CHECK_FALSE(back.autoBackup);
    CHECK(back.user.empty());
    CHECK(back.password.empty());
    CHECK(core::parseKept("").root == "/3DSaveSync");
    CHECK(core::parseKept("").autoBackup);
    CHECK(core::parseKept("user=a\npassword=b\nroot=Y/\n").root == "/Y");
}

TEST_CASE("normalizeRoot") {
    CHECK(core::normalizeRoot("3DSaveSync/") == "/3DSaveSync");
    CHECK(core::normalizeRoot("/a/b//") == "/a/b");
    CHECK(core::normalizeRoot("") == "/3DSaveSync");
    CHECK(core::normalizeRoot("/") == "/3DSaveSync");
}
