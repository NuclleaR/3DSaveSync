#include "core/backup.hpp"
#include "core/error.hpp"
#include "doctest.h"

using core::BackupSummary;
using core::GameResult;
using core::Outcome;

namespace {

GameResult result(Outcome o) {
    return {core::GameInfo{0, "0x00001 Game", "Game"}, o, std::nullopt};
}

}  // namespace

TEST_CASE("errors on the console are not reported as Koofr errors") {
    CHECK(core::describe({core::ErrorKind::Io, "Reading /main failed (0xC8804478)"}) ==
          "Console storage error: Reading /main failed (0xC8804478)");
}

TEST_CASE("counts line: empty games count as unchanged, skipped ones are left out") {
    BackupSummary s;
    s.results = {result(Outcome::Uploaded), result(Outcome::Unchanged), result(Outcome::Empty),
                 result(Outcome::Failed),   result(Outcome::Skipped),   result(Outcome::Uploaded)};
    CHECK(core::countsLine(s) == "2 uploaded · 2 unchanged · 1 errors");
}

TEST_CASE("the app never backs up its own save (it holds the login)") {
    const uint64_t self = 0x000400000F3D5500ULL;
    CHECK_FALSE(core::isBackupCandidate(self, self));
    CHECK_FALSE(core::isBackupCandidate(0x0004000E00175E00ULL, self));  // an update title
    CHECK(core::isBackupCandidate(0x0004000000175E00ULL, self));
}

TEST_CASE("counts line of an empty run") {
    CHECK(core::countsLine(BackupSummary{}) == "0 uploaded · 0 unchanged · 0 errors");
}
