#include "third_party/doctest.h"

#include <functional>
#include <map>
#include <optional>

#include "cloud_fixtures.hpp"
#include "core/backup.hpp"

namespace {

using Versions = std::vector<std::string>;

class FakeGames : public core::GameSource {
public:
    struct Game {
        core::GameInfo info;
        std::vector<TestFile> files;
        std::optional<core::Error> snapshotError;
    };

    std::vector<Game> list;

    void add(const std::string& folder, std::vector<TestFile> files) {
        list.push_back({{static_cast<uint64_t>(list.size() + 1), folder, folder}, std::move(files), std::nullopt});
    }
    Game& game(const std::string& folder) {
        for (auto& g : list) {
            if (g.info.folderName == folder) return g;
        }
        FAIL("no such game " << folder);
        return list.front();
    }

    std::expected<std::vector<core::GameInfo>, core::Error> games() override {
        std::vector<core::GameInfo> out;
        for (const auto& g : list) out.push_back(g.info);
        return out;
    }
    std::expected<core::Manifest, core::Error> snapshot(const core::GameInfo& info) override {
        auto& g = game(info.folderName);
        if (g.snapshotError) return std::unexpected(*g.snapshotError);
        return makeManifest(info.titleId, g.files);
    }
    core::FileSource& files(const core::GameInfo& info) override {
        auto& mem = mem_[info.folderName];
        mem.set(game(info.folderName).files);
        return mem;
    }

private:
    std::map<std::string, MemFiles> mem_;
};

struct Progress : core::BackupProgress {
    std::vector<size_t> started;
    std::vector<core::Outcome> finished;
    std::function<void(size_t)> onFinished;
    void gameStarted(size_t index, size_t, const core::GameInfo&) override { started.push_back(index); }
    void gameFinished(size_t index, const core::GameResult& r) override {
        finished.push_back(r.outcome);
        if (onFinished) onFinished(index);
    }
};

struct Fixture {
    FakeDav dav;
    core::CloudStore cs{dav, "/R"};
    FakeGames games;
    Progress progress;
    core::CancelToken cancel;
    Fixture() {
        games.add("G1", {{core::Part::Save, "main", "g1-save"}, {core::Part::Extdata, "ext", "g1-ext"}});
        games.add("G2", {{core::Part::Save, "main", "g2-save"}});
    }
    core::BackupSummary run(const std::string& version) { return core::runBackup(games, cs, version, progress, cancel); }
};

}  // namespace

TEST_CASE_FIXTURE(Fixture, "first run uploads, second run is unchanged") {
    auto s1 = run("V1");
    CHECK(s1.count(core::Outcome::Uploaded) == 2);
    CHECK_FALSE(s1.fatal);
    auto s2 = run("V2");
    CHECK(s2.count(core::Outcome::Unchanged) == 2);
    CHECK(cs.listVersions("G1").value() == Versions{"V1"});
    CHECK(progress.started == std::vector<size_t>{0, 1, 0, 1});
}

TEST_CASE_FIXTURE(Fixture, "only a changed game gets a new version") {
    run("V1");
    games.game("G2").files[0].content = "g2-save-changed";
    auto s = run("V2");
    CHECK(s.results[0].outcome == core::Outcome::Unchanged);
    CHECK(s.results[1].outcome == core::Outcome::Uploaded);
    CHECK(cs.listVersions("G2").value() == Versions{"V2", "V1"});
    CHECK(cs.listVersions("G1").value() == Versions{"V1"});
}

TEST_CASE_FIXTURE(Fixture, "a changed save uploads the full snapshot") {
    run("V1");
    games.game("G1").files[0].content = "g1-save-changed";
    run("V2");
    CHECK(dav.isFile("/R/extdata/G1/V2/ext"));
    CHECK(dav.content("/R/saves/G1/V2/main") == "g1-save-changed");
}

TEST_CASE_FIXTURE(Fixture, "same-second rerun gets a suffixed name") {
    run("V1");
    games.game("G1").files[0].content = "again";
    run("V1");
    CHECK(cs.listVersions("G1").value() == Versions{"V1-2", "V1"});
}

TEST_CASE_FIXTURE(Fixture, "empty snapshot is not uploaded") {
    games.game("G2").files.clear();
    auto s = run("V1");
    CHECK(s.results[1].outcome == core::Outcome::Empty);
    CHECK(cs.listVersions("G2").value().empty());
}

TEST_CASE_FIXTURE(Fixture, "unreadable game fails alone") {
    games.game("G1").snapshotError = core::Error{core::ErrorKind::Io, "read failed"};
    auto s = run("V1");
    CHECK(s.results[0].outcome == core::Outcome::Failed);
    REQUIRE(s.results[0].error);
    CHECK(s.results[0].error->kind == core::ErrorKind::Io);
    CHECK(s.results[1].outcome == core::Outcome::Uploaded);
    CHECK_FALSE(s.fatal);
}

TEST_CASE_FIXTURE(Fixture, "network error on first game stops the run") {
    dav.failOn("PROPFIND", "/R/manifests/G1", 1, core::ErrorKind::Network);
    auto s = run("V1");
    REQUIRE(s.fatal);
    CHECK(s.fatal->kind == core::ErrorKind::Network);
    CHECK(s.results[0].outcome == core::Outcome::Failed);
    CHECK(s.results[1].outcome == core::Outcome::Skipped);
    CHECK(s.count(core::Outcome::Uploaded) == 0);
}

TEST_CASE_FIXTURE(Fixture, "quota error stops the run") {
    dav.failOn("PUT", "/R/.incoming/G1", 1, core::ErrorKind::Quota);
    auto s = run("V1");
    REQUIRE(s.fatal);
    CHECK(s.fatal->kind == core::ErrorKind::Quota);
    CHECK(s.results[1].outcome == core::Outcome::Skipped);
    CHECK(cs.listVersions("G1").value().empty());
}

TEST_CASE_FIXTURE(Fixture, "auth error stops the run") {
    dav.failOn("PROPFIND", "/R/manifests/G1", 1, core::ErrorKind::Auth);
    auto s = run("V1");
    REQUIRE(s.fatal);
    CHECK(s.fatal->kind == core::ErrorKind::Auth);
    CHECK(s.results[1].outcome == core::Outcome::Skipped);
}

TEST_CASE_FIXTURE(Fixture, "cancel stops before the next game") {
    progress.onFinished = [this](size_t index) {
        if (index == 0) cancel.requested = true;
    };
    auto s = run("V1");
    CHECK(s.results[0].outcome == core::Outcome::Uploaded);
    CHECK(s.results[1].outcome == core::Outcome::Skipped);
    REQUIRE(s.fatal);
    CHECK(s.fatal->kind == core::ErrorKind::Cancelled);
}
