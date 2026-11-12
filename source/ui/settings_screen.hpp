#pragma once

#include <array>
#include <optional>
#include <string>

#include "ui/app.hpp"

namespace ui {

// Email, app password, cloud folder, Test connection, Save, Sign out.
// Changes are kept in a draft until Save has checked them with Koofr.
class SettingsScreen {
public:
    explicit SettingsScreen(App& app);
    void enter();
    // Setup after Sign out; Menu on B.
    ScreenId update(u32 down, const touchPosition* touch);
    void draw();

private:
    enum class Phase { Edit, Checking, ConfirmSignOut };
    // kAutoBackup is the checkbox row under the 2×3 button grid.
    enum Action { kEmail, kPassword, kFolder, kTest, kSave, kSignOut, kAutoBackup, kActionCount };

    void press(int action);
    void startCheck(bool save);
    void finishCheck();

    App& app_;
    Mutex mutex_;
    std::optional<std::optional<core::Error>> result_;  // set by the worker: the check's outcome
    std::optional<core::Error> saveError_;              // set by the worker when saving failed
    bool saving_ = false;
    Phase phase_ = Phase::Edit;
    core::Settings draft_;
    std::array<Button, kActionCount> buttons_;  // the last one is the checkbox row's touch area
    Button confirm_;
    Button back_;
    int focus_ = 0;
    std::string message_;
    u32 messageColor_ = kMuted;
};

}  // namespace ui
