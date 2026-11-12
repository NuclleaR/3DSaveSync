#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/restore.hpp"
#include "ui/app.hpp"
#include "ui/game_list.hpp"

namespace ui {

// Restore: pick a game (top), pick a version (bottom), download it into
// Checkpoint's folders. The owner then restores it with Checkpoint.
class RestoreScreen {
public:
    explicit RestoreScreen(App& app);
    // Loads the games and versions from Koofr on the worker.
    void enter();
    ScreenId update(u32 down, const touchPosition* touch);
    void draw();

private:
    enum class Phase { Loading, Pick, Working };

    // Shared with the worker; guarded by mutex.
    struct Shared {
        size_t loaded = 0;
        size_t total = 0;
        std::optional<std::vector<CloudTitle>> titles;
        std::optional<core::RestoreResult> restored;
        std::optional<core::Error> error;
    };

    void showVersions();
    void startDownload();
    void finishJob();

    App& app_;
    Mutex mutex_;
    Shared shared_;
    Phase phase_ = Phase::Loading;
    bool leaving_ = false;     // B pressed while loading: back to the menu when it ends
    bool onVersions_ = false;  // the D-pad drives the version list
    GameListView games_;
    ListView versions_;
    Button download_;
    Button cancel_;
    std::string working_;  // what the running job does
    std::string message_;
    u32 messageColor_ = kMuted;
};

}  // namespace ui
