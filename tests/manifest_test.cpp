#include "third_party/doctest.h"

#include "core/manifest.hpp"

static core::Manifest sample() {
    return {0x0004000000175E00ULL,
            {{core::Part::Extdata, "user/data 1.bin", 512, std::string(64, 'b')},
             {core::Part::Save, "main", 1024, std::string(64, 'a')}},
            {}};
}

TEST_CASE("manifest round trip and order") {
    auto text = core::serializeManifest(sample());
    CHECK(text.rfind("3dsavesync-manifest 1\ntitle 0004000000175E00\nsave\t1024\t", 0) == 0);
    auto back = core::parseManifest(text);
    REQUIRE(back);
    CHECK(back->titleId == 0x0004000000175E00ULL);
    CHECK(back->files.size() == 2);
    CHECK(back->files[1].path == "user/data 1.bin");
    CHECK(core::sameContent(*back, sample()));
}

TEST_CASE("manifest rejects garbage") {
    CHECK_FALSE(core::parseManifest("hello"));
    CHECK_FALSE(core::parseManifest("3dsavesync-manifest 2\ntitle 0\n"));
    CHECK_FALSE(core::parseManifest("3dsavesync-manifest 1\ntitle 0\nsave\tx\ty\tz\n"));
}

TEST_CASE("manifest keeps folders, so empty ones come back on restore") {
    auto m = sample();
    m.dirs = {{core::Part::Extdata, "user"}, {core::Part::Save, "sub/empty"}};
    const auto text = core::serializeManifest(m);
    CHECK(text.find("savedir\tsub/empty\n") != std::string::npos);
    CHECK(text.find("extdatadir\tuser\n") != std::string::npos);
    auto back = core::parseManifest(text);
    REQUIRE(back);
    REQUIRE(back->dirs.size() == 2);
    CHECK(back->dirs[0].part == core::Part::Save);  // Save first, then by path
    CHECK(back->dirs[0].path == "sub/empty");
    CHECK(back->dirs[1].path == "user");
    CHECK(core::sameContent(*back, m));
}

TEST_CASE("a manifest without folder lines has no folders") {
    auto back = core::parseManifest(core::serializeManifest(sample()));
    REQUIRE(back);
    CHECK(back->dirs.empty());
    CHECK_FALSE(core::parseManifest("3dsavesync-manifest 1\ntitle 0\nsavedir\t\n"));
}

TEST_CASE("sameContent sees an added or removed folder") {
    auto withDir = sample();
    withDir.dirs = {{core::Part::Save, "empty"}};
    CHECK_FALSE(core::sameContent(sample(), withDir));
    CHECK_FALSE(core::sameContent(withDir, sample()));
}

TEST_CASE("sameContent detects changes") {
    auto changed = sample();
    changed.files[0].sha256 = std::string(64, 'c');
    CHECK_FALSE(core::sameContent(sample(), changed));
    auto added = sample();
    added.files.push_back({core::Part::Save, "extra", 1, std::string(64, 'd')});
    CHECK_FALSE(core::sameContent(sample(), added));
    CHECK(core::hasPart(sample(), core::Part::Extdata));
}
