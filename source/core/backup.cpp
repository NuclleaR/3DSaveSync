#include "core/backup.hpp"

#include <algorithm>

#include "core/checkpoint.hpp"

namespace core {

namespace {

GameResult failed(const GameInfo& g, Error e) {
    return {g, Outcome::Failed, std::move(e)};
}

GameResult backupGame(GameSource& games, CloudStore& cloud, const GameInfo& g, const std::string& runVersion,
                      const CancelToken& cancel) {
    auto snapshot = games.snapshot(g);
    if (!snapshot) {
        return failed(g, snapshot.error());
    }
    if (snapshot->files.empty()) {
        return {g, Outcome::Empty, std::nullopt};
    }
    auto versions = cloud.listVersions(g.folderName);
    if (!versions) {
        return failed(g, versions.error());
    }
    if (!versions->empty()) {
        auto latest = cloud.manifest(g.folderName, versions->front());
        if (!latest) {
            return failed(g, latest.error());
        }
        if (sameContent(*latest, *snapshot)) {
            return {g, Outcome::Unchanged, std::nullopt};
        }
    }
    const std::string version = uniqueName(runVersion, *versions);
    if (auto r = cloud.uploadVersion(g.folderName, version, *snapshot, games.files(g), cancel); !r) {
        return failed(g, r.error());
    }
    return {g, Outcome::Uploaded, std::nullopt};
}

}  // namespace

int BackupSummary::count(Outcome o) const {
    return static_cast<int>(std::count_if(results.begin(), results.end(), [o](const GameResult& r) { return r.outcome == o; }));
}

bool isBackupCandidate(uint64_t titleId, uint64_t ownTitleId) {
    return titleId != ownTitleId && !isSystemExcluded(titleId);
}

std::string countsLine(const BackupSummary& s) {
    const int unchanged = s.count(Outcome::Unchanged) + s.count(Outcome::Empty);
    return std::to_string(s.count(Outcome::Uploaded)) + " uploaded · " + std::to_string(unchanged) +
           " unchanged · " + std::to_string(s.count(Outcome::Failed)) + " errors";
}

BackupSummary runBackup(GameSource& games, CloudStore& cloud, const std::string& runVersion, BackupProgress& progress,
                        const CancelToken& cancel) {
    BackupSummary summary;
    auto list = games.games();
    if (!list) {
        summary.fatal = list.error();
        return summary;
    }
    for (const auto& g : *list) {
        summary.results.push_back({g, Outcome::Skipped, std::nullopt});
    }
    for (size_t i = 0; i < list->size(); ++i) {
        if (cancel.cancelled()) {
            summary.fatal = Error{ErrorKind::Cancelled, "backup cancelled"};
            break;
        }
        const GameInfo& g = (*list)[i];
        progress.gameStarted(i, list->size(), g);
        summary.results[i] = backupGame(games, cloud, g, runVersion, cancel);
        progress.gameFinished(i, summary.results[i]);
        const auto& error = summary.results[i].error;
        if (error && isFatal(*error)) {
            summary.fatal = *error;
            break;
        }
    }
    return summary;
}

}  // namespace core
