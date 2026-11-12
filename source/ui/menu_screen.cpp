#include "ui/menu_screen.hpp"

namespace ui {

namespace {

constexpr std::array<ScreenId, 4> kTargets{ScreenId::Restore, ScreenId::Delete, ScreenId::Sync, ScreenId::Settings};

}  // namespace

MenuScreen::MenuScreen(App& app, SyncScreen& sync)
    : app_(app),
      sync_(sync),
      buttons_{{
          {10.0f, 20.0f, 145.0f, 36.0f, "Restore", true},
          {165.0f, 20.0f, 145.0f, 36.0f, "Delete versions"},
          {10.0f, 64.0f, 145.0f, 36.0f, "Sync again"},
          {165.0f, 64.0f, 145.0f, 36.0f, "Settings"},
      }} {}

ScreenId MenuScreen::update(u32 down, const touchPosition* touch) {
    if (down & KEY_START) {
        return ScreenId::Exit;
    }
    // 2×2 grid: Left/Right switch the column, Up/Down the row.
    if (down & (KEY_DLEFT | KEY_DRIGHT)) {
        focus_ ^= 1;
    }
    if (down & (KEY_DUP | KEY_DDOWN)) {
        focus_ ^= 2;
    }
    if (down & KEY_A) {
        return kTargets[focus_];
    }
    if (touch != nullptr) {
        for (size_t i = 0; i < buttons_.size(); ++i) {
            if (Ui::hit(buttons_[i], *touch)) {
                return kTargets[i];
            }
        }
    }
    return ScreenId::Menu;
}

void MenuScreen::draw() {
    sync_.drawSummary();
    for (size_t i = 0; i < buttons_.size(); ++i) {
        Button b = buttons_[i];
        if (kTargets[i] == ScreenId::Sync && !sync_.ran()) {
            b.label = "Sync now";
        }
        app_.ui.button(b, static_cast<int>(i) == focus_);
    }
    // Which Koofr account and folder the backups go to.
    app_.ui.centered(GFX_BOTTOM, 160.0f, app_.ui.fit(app_.settings.user, kBottomWidth - 20.0f, 0.45f), kMuted, 0.45f);
    app_.ui.centered(GFX_BOTTOM, 176.0f, app_.ui.fit(app_.settings.root, kBottomWidth - 20.0f, 0.45f), kMuted, 0.45f);
    footHint(app_.ui, "START — exit");
}

}  // namespace ui
