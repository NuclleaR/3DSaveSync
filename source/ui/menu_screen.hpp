#pragma once

#include <array>

#include "ui/app.hpp"
#include "ui/sync_screen.hpp"

namespace ui {

// The sync summary on top, Restore / Delete versions / Sync again / Settings below.
class MenuScreen {
public:
    MenuScreen(App& app, SyncScreen& sync);
    ScreenId update(u32 down, const touchPosition* touch);
    void draw();

private:
    App& app_;
    SyncScreen& sync_;
    std::array<Button, 4> buttons_;
    int focus_ = 0;
};

}  // namespace ui
