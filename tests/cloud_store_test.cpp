#include "third_party/doctest.h"

#include "cloud_fixtures.hpp"

using Versions = std::vector<std::string>;

TEST_CASE("upload creates the final layout and no incoming leftovers") {
    FakeDav dav;
    core::CloudStore cs(dav, "/R");
    MemFiles files(sampleFiles());
    const core::CancelToken cancel;
    REQUIRE(cs.uploadVersion("0x0175E Pokémon Moon", "20261112-194005", makeManifest(1, sampleFiles()), files, cancel));
    CHECK(dav.isFile("/R/saves/0x0175E Pokémon Moon/20261112-194005/main"));
    CHECK(dav.content("/R/saves/0x0175E Pokémon Moon/20261112-194005/main") == "save-bytes");
    CHECK(dav.isFile("/R/extdata/0x0175E Pokémon Moon/20261112-194005/user/data 1.bin"));
    CHECK(dav.isFile("/R/manifests/0x0175E Pokémon Moon/20261112-194005.manifest"));
    CHECK_FALSE(dav.exists("/R/.incoming/0x0175E Pokémon Moon/20261112-194005"));
}

TEST_CASE("no extdata folder when the snapshot has none") {
    FakeDav dav;
    core::CloudStore cs(dav, "/R");
    MemFiles files(twoSaveFiles());
    const core::CancelToken cancel;
    REQUIRE(cs.uploadVersion("T", "V", makeManifest(1, twoSaveFiles()), files, cancel));
    CHECK(dav.isFile("/R/saves/T/V/second"));
    CHECK_FALSE(dav.exists("/R/extdata/T/V"));
}

TEST_CASE("listVersions only counts manifests, newest first") {
    FakeDav dav;
    core::CloudStore cs(dav, "/R");
    putCompleteVersion(dav, "T", "V1");
    putCompleteVersion(dav, "T", "V3");
    dav.mkfile("/R/saves/T/V2/main");
    CHECK(cs.listVersions("T").value() == Versions{"V3", "V1"});
    CHECK(cs.listTitles().value() == Versions{"T"});
    CHECK(cs.listVersions("missing").value().empty());
    CHECK(cs.latestManifest("missing").value() == std::nullopt);
    auto latest = cs.latestManifest("T");
    REQUIRE(latest);
    REQUIRE(latest->has_value());
    CHECK((*latest)->files.at(0).path == "main");
}

TEST_CASE("listTitles is empty before the first backup") {
    FakeDav dav;
    core::CloudStore cs(dav, "/R");
    CHECK(cs.listTitles().value().empty());
}

TEST_CASE("failed upload leaves no complete version") {
    FakeDav dav;
    core::CloudStore cs(dav, "/R");
    MemFiles files(twoSaveFiles());
    const core::CancelToken cancel;
    dav.failOn("PUT", "/R/.incoming/T/V/saves/second", 1, core::ErrorKind::Quota);
    auto r = cs.uploadVersion("T", "V", makeManifest(1, twoSaveFiles()), files, cancel);
    REQUIRE_FALSE(r);
    CHECK(r.error().kind == core::ErrorKind::Quota);
    CHECK(cs.listVersions("T").value().empty());
}

TEST_CASE("cleanup removes partial moves of an interrupted upload only") {
    FakeDav dav;
    core::CloudStore cs(dav, "/R");
    MemFiles files(sampleFiles());
    const core::CancelToken cancel;
    dav.failOn("MOVE", "/R/.incoming/T/V/V.manifest", 1, core::ErrorKind::Network);  // saves and extdata already moved
    CHECK_FALSE(cs.uploadVersion("T", "V", makeManifest(1, sampleFiles()), files, cancel));
    CHECK(dav.exists("/R/saves/T/V"));
    dav.mkfile("/R/saves/T/ORPHAN/main");  // no .incoming counterpart
    putCompleteVersion(dav, "T", "OLD");
    REQUIRE(cs.cleanupIncomplete());
    CHECK_FALSE(dav.exists("/R/saves/T/V"));
    CHECK_FALSE(dav.exists("/R/extdata/T/V"));
    CHECK_FALSE(dav.exists("/R/.incoming/T/V"));
    CHECK(dav.exists("/R/saves/T/ORPHAN/main"));
    CHECK(dav.exists("/R/saves/T/OLD/main"));
    CHECK(cs.listVersions("T").value() == Versions{"OLD"});
}

TEST_CASE("cleanup keeps a version that was completed before the crash") {
    FakeDav dav;
    core::CloudStore cs(dav, "/R");
    MemFiles files(sampleFiles());
    const core::CancelToken cancel;
    dav.failOn("DELETE", "/R/.incoming/T/V", 1, core::ErrorKind::Network);
    CHECK_FALSE(cs.uploadVersion("T", "V", makeManifest(1, sampleFiles()), files, cancel));
    REQUIRE(cs.cleanupIncomplete());
    CHECK(cs.listVersions("T").value() == Versions{"V"});
    CHECK(dav.isFile("/R/saves/T/V/main"));
    CHECK_FALSE(dav.exists("/R/.incoming/T/V"));
}

TEST_CASE("cleanup deletes nothing when listing fails") {
    FakeDav dav;
    core::CloudStore cs(dav, "/R");
    dav.mkfile("/R/.incoming/T/V/saves/main");
    dav.failOn("PROPFIND", "/R/.incoming", 1, core::ErrorKind::Network);
    CHECK_FALSE(cs.cleanupIncomplete());
    CHECK(dav.deletes == 0);
}

TEST_CASE("cleanup deletes nothing when the manifest folder cannot be listed") {
    FakeDav dav;
    core::CloudStore cs(dav, "/R");
    putCompleteVersion(dav, "T", "V");
    dav.mkfile("/R/.incoming/T/V/saves/main");
    dav.failOn("PROPFIND", "/R/manifests/T", 1, core::ErrorKind::Network);
    CHECK_FALSE(cs.cleanupIncomplete());
    CHECK(dav.deletes == 0);
    CHECK(dav.exists("/R/saves/T/V/main"));
}

TEST_CASE("delete removes manifest last") {
    FakeDav dav;
    core::CloudStore cs(dav, "/R");
    putCompleteVersion(dav, "T", "V");
    dav.failOn("DELETE", "/R/manifests/T/V.manifest", 1, core::ErrorKind::Network);
    CHECK_FALSE(cs.deleteVersion("T", "V"));
    CHECK(cs.listVersions("T").value() == Versions{"V"});
    REQUIRE(cs.deleteVersion("T", "V"));
    CHECK(cs.listVersions("T").value().empty());
    CHECK_FALSE(dav.exists("/R/saves/T/V"));
}

TEST_CASE("cancel before a file stops the upload") {
    FakeDav dav;
    core::CloudStore cs(dav, "/R");
    MemFiles files(sampleFiles());
    core::CancelToken cancel;
    cancel.requested = true;
    auto r = cs.uploadVersion("T", "V", makeManifest(1, sampleFiles()), files, cancel);
    REQUIRE_FALSE(r);
    CHECK(r.error().kind == core::ErrorKind::Cancelled);
    CHECK(cs.listVersions("T").value().empty());
}

TEST_CASE("download reads the file of the right part") {
    FakeDav dav;
    core::CloudStore cs(dav, "/R");
    MemFiles files(sampleFiles());
    const core::CancelToken cancel;
    const auto m = makeManifest(1, sampleFiles());
    REQUIRE(cs.uploadVersion("T", "V", m, files, cancel));
    struct Sink : core::BodySink {
        std::string data;
        std::expected<void, core::Error> write(const char* d, size_t n) override {
            data.append(d, n);
            return {};
        }
    } sink;
    const auto& ext = m.files[1].part == core::Part::Extdata ? m.files[1] : m.files[0];
    REQUIRE(cs.download("T", "V", ext, sink));
    CHECK(sink.data == "ext-bytes");
    CHECK(cs.manifest("T", "V").value().files.size() == 2);
}
