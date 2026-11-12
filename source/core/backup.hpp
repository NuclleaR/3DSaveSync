#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "core/cancel.hpp"
#include "core/cloud_store.hpp"
#include "core/error.hpp"
#include "core/manifest.hpp"

namespace core {

struct GameInfo {
    uint64_t titleId;
    std::string folderName;   // Checkpoint title folder, e.g. "0x0175E Pokémon Moon"
    std::string displayName;  // unsanitized name for the UI
};

class GameSource {
public:
    virtual ~GameSource() = default;
    virtual std::expected<std::vector<GameInfo>, Error> games() = 0;
    // Reads and hashes the game's save data and extdata.
    virtual std::expected<Manifest, Error> snapshot(const GameInfo& g) = 0;
    virtual FileSource& files(const GameInfo& g) = 0;
};

// Skipped: not attempted because the run stopped (fatal error or cancel).
enum class Outcome { Uploaded, Unchanged, Empty, Failed, Skipped };

struct GameResult {
    GameInfo game;
    Outcome outcome;
    std::optional<Error> error;
};

struct BackupSummary {
    std::vector<GameResult> results;  // one per game, in order
    std::optional<Error> fatal;       // why the run stopped early, if it did
    int count(Outcome o) const;
};

class BackupProgress {
public:
    virtual ~BackupProgress() = default;
    virtual void gameStarted(size_t index, size_t total, const GameInfo& g) = 0;
    virtual void gameFinished(size_t index, const GameResult& r) = 0;
};

// Backs up every game: a game whose snapshot equals its latest complete
// version is Unchanged; otherwise its full snapshot is uploaded as
// uniqueName(runVersion, existing versions). A non-fatal error fails only
// that game; a fatal one (isFatal) or cancel stops the run.
BackupSummary runBackup(GameSource& games, CloudStore& cloud, const std::string& runVersion, BackupProgress& progress,
                        const CancelToken& cancel);

}  // namespace core
