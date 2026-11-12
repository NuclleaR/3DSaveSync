#include "third_party/doctest.h"

#include "cloud_fixtures.hpp"
#include "core/restore.hpp"
#include "fake_fs.hpp"

namespace {

const std::string kTitle = "0x0175E Pokémon Moon";

struct Fixture {
    FakeDav dav;
    core::CloudStore cs{dav, "/R"};
    FakeFs fs;
    core::CancelToken cancel;
    core::HasherFactory hasher = fakeHasherFactory();

    void upload(const std::string& title, const std::string& version, const std::vector<TestFile>& files) {
        MemFiles mem(files);
        REQUIRE(cs.uploadVersion(title, version, makeManifest(1, files), mem, cancel));
    }
};

}  // namespace

TEST_CASE_FIXTURE(Fixture, "restore writes save and extdata into Checkpoint folders") {
    upload(kTitle, "V1", sampleFiles());
    auto r = core::restoreVersion(cs, fs, hasher, kTitle, "V1", cancel);
    REQUIRE(r);
    CHECK(r->localVersion == "V1");
    CHECK(fs.file("/3ds/Checkpoint/saves/0x0175E Pokémon Moon/V1/main") == "save-bytes");
    CHECK(fs.file("/3ds/Checkpoint/extdata/0x0175E Pokémon Moon/V1/user/data 1.bin") == "ext-bytes");
}

TEST_CASE_FIXTURE(Fixture, "existing local folder is never overwritten") {
    upload("T", "V1", sampleFiles());
    fs.mkfile("/3ds/Checkpoint/saves/T/V1/main", "mine");
    auto r = core::restoreVersion(cs, fs, hasher, "T", "V1", cancel);
    REQUIRE(r);
    CHECK(r->localVersion == "V1-2");
    CHECK(fs.file("/3ds/Checkpoint/saves/T/V1/main") == "mine");
    CHECK(fs.file("/3ds/Checkpoint/saves/T/V1-2/main") == "save-bytes");
}

TEST_CASE_FIXTURE(Fixture, "empty folders of the version are created") {
    const auto files = sampleFiles();
    auto m = makeManifest(1, files);
    m.dirs = {{core::Part::Save, "sub/empty"}, {core::Part::Extdata, "user"}, {core::Part::Extdata, "cache"}};
    MemFiles mem(files);
    REQUIRE(cs.uploadVersion("T", "V1", m, mem, cancel));
    REQUIRE(core::restoreVersion(cs, fs, hasher, "T", "V1", cancel));
    CHECK(fs.dirs.count("/3ds/Checkpoint/saves/T/V1/sub/empty") == 1);
    CHECK(fs.dirs.count("/3ds/Checkpoint/extdata/T/V1/cache") == 1);
    CHECK(fs.file("/3ds/Checkpoint/extdata/T/V1/user/data 1.bin") == "ext-bytes");
}

TEST_CASE_FIXTURE(Fixture, "an unreadable local folder stops the restore before anything is written") {
    upload("T", "V1", sampleFiles());
    fs.mkfile("/3ds/Checkpoint/saves/T/V1/main", "mine");
    fs.unlistable.insert("/3ds/Checkpoint/saves/T");
    auto r = core::restoreVersion(cs, fs, hasher, "T", "V1", cancel);
    REQUIRE_FALSE(r);
    CHECK(r.error().kind == core::ErrorKind::Io);
    CHECK(fs.file("/3ds/Checkpoint/saves/T/V1/main") == "mine");
    CHECK_FALSE(fs.exists("/3ds/Checkpoint/extdata/T"));
}

TEST_CASE_FIXTURE(Fixture, "a name taken only in extdata is also avoided") {
    upload("T", "V1", sampleFiles());
    fs.mkfile("/3ds/Checkpoint/extdata/T/V1/x", "mine");
    CHECK(core::restoreVersion(cs, fs, hasher, "T", "V1", cancel)->localVersion == "V1-2");
}

TEST_CASE_FIXTURE(Fixture, "no extdata folder for a save-only version") {
    upload("T", "V1", twoSaveFiles());
    REQUIRE(core::restoreVersion(cs, fs, hasher, "T", "V1", cancel));
    CHECK(fs.file("/3ds/Checkpoint/saves/T/V1/second") == "two");
    CHECK_FALSE(fs.exists("/3ds/Checkpoint/extdata/T/V1"));
}

TEST_CASE_FIXTURE(Fixture, "corrupted download leaves nothing behind") {
    upload("T", "V1", sampleFiles());
    dav.corrupt("/R/saves/T/V1/main");
    auto r = core::restoreVersion(cs, fs, hasher, "T", "V1", cancel);
    REQUIRE_FALSE(r);
    CHECK(r.error().detail == "Downloaded file is corrupted");
    CHECK_FALSE(fs.exists("/3ds/Checkpoint/saves/T/V1"));
    CHECK_FALSE(fs.exists("/3ds/Checkpoint/extdata/T/V1"));
}

TEST_CASE_FIXTURE(Fixture, "network failure mid-restore leaves nothing behind") {
    upload("T", "V1", sampleFiles());
    dav.failOn("GET", "/R/extdata/T/V1", 1, core::ErrorKind::Network);
    auto r = core::restoreVersion(cs, fs, hasher, "T", "V1", cancel);
    REQUIRE_FALSE(r);
    CHECK(r.error().kind == core::ErrorKind::Network);
    CHECK_FALSE(fs.exists("/3ds/Checkpoint/saves/T/V1"));
}
