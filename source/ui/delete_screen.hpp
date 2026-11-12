#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ui/app.hpp"
#include "ui/game_list.hpp"

namespace ui {

// Delete versions: pick a game (top), tick versions (bottom), confirm. Only
// the versions the owner ticked are deleted.
class DeleteScreen {
public:
    explicit DeleteScreen(App& app);
    void enter();
    ScreenId update(u32 down, const touchPosition* touch);
    void draw();

private:
    enum class Phase { Loading, Pick, Confirm, Working };

    struct Shared {
        size_t done = 0;
        size_t total = 0;
        std::optional<std::vector<CloudTitle>> titles;
        std::optional<std::vector<std::string>> versionsAfter;  // the title's versions after deleting
        size_t failed = 0;
        std::optional<core::Error> error;
    };

    void showVersions();
    size_t selectedCount() const;
    void startDelete();
    void finishJob();

    App& app_;
    Mutex mutex_;
    Shared shared_;
    Phase phase_ = Phase::Loading;
    bool leaving_ = false;
    bool onVersions_ = false;
    bool onButton_ = false;  // the D-pad is on "Delete N versions"
    GameListView games_;
    ListView versions_;
    Button delete_;
    Button confirm_;
    Button back_;
    std::string message_;
    u32 messageColor_ = kMuted;
    size_t deleting_ = 0;
};

}  // namespace ui
