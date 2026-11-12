#include "ui/settings_screen.hpp"

#include <mutex>

#include "platform/keyboard.hpp"
#include "platform/secure_store.hpp"

namespace ui {

SettingsScreen::SettingsScreen(App& app)
    : app_(app),
      buttons_{{
          {10.0f, 20.0f, 145.0f, 36.0f, "Email"},
          {165.0f, 20.0f, 145.0f, 36.0f, "App password"},
          {10.0f, 64.0f, 145.0f, 36.0f, "Cloud folder"},
          {165.0f, 64.0f, 145.0f, 36.0f, "Test connection"},
          {10.0f, 108.0f, 145.0f, 36.0f, "Save", true},
          {165.0f, 108.0f, 145.0f, 36.0f, "Sign out", false, true},
          {10.0f, 152.0f, 300.0f, 28.0f, "Back up on start"},
      }},
      confirm_{10.0f, 80.0f, 145.0f, 36.0f, "A — Sign out", false, true},
      back_{165.0f, 80.0f, 145.0f, 36.0f, "B — Cancel"} {}

void SettingsScreen::enter() {
    draft_ = app_.settings;
    phase_ = Phase::Edit;
    focus_ = 0;
    message_.clear();
}

void SettingsScreen::startCheck(bool save) {
    {
        std::lock_guard lock(mutex_);
        result_.reset();
        saveError_.reset();
    }
    saving_ = save;
    app_.cancel.requested = false;
    const core::Settings s = draft_;
    const bool started = app_.worker.start([this, s, save] {
        auto err = checkLogin(s, &app_.cancel);
        std::optional<core::Error> saveErr;
        if (!err && save) {
            if (auto saved = platform::saveSettings(s); !saved) {
                saveErr = saved.error();
            }
        }
        std::lock_guard lock(mutex_);
        result_ = err;
        saveError_ = saveErr;
    });
    if (!started) {
        message_ = "Could not start the background task.";
        messageColor_ = kError;
        return;
    }
    message_ = "Checking the login with Koofr…";
    messageColor_ = kCurrent;
    phase_ = Phase::Checking;
}

void SettingsScreen::finishCheck() {
    std::lock_guard lock(mutex_);
    phase_ = Phase::Edit;
    if (!result_) {
        return;
    }
    if (*result_) {
        // Koofr refused: the saved settings stay as they were.
        message_ = core::describe(**result_);
        messageColor_ = kError;
    } else if (saveError_) {
        message_ = "Could not save the settings: " + saveError_->detail;
        messageColor_ = kError;
    } else if (saving_) {
        app_.settings = draft_;
        message_ = "Saved.";
        messageColor_ = kOk;
    } else {
        message_ = "Connected to Koofr";
        messageColor_ = kOk;
    }
}

void SettingsScreen::press(int action) {
    switch (action) {
        case kEmail:
            if (auto t = platform::askText("Koofr email", draft_.user, false)) {
                draft_.user = *t;
            }
            break;
        case kPassword:
            if (auto t = platform::askText("Koofr app password", "", true)) {
                draft_.password = *t;
            }
            break;
        case kFolder:
            if (auto t = platform::askText("Cloud folder", draft_.root, false)) {
                draft_.root = core::normalizeRoot(*t);
            }
            break;
        case kTest: startCheck(false); break;
        case kSave: startCheck(true); break;
        case kSignOut: phase_ = Phase::ConfirmSignOut; break;
        case kAutoBackup: {
            // Saved at once and without a Koofr check: it is a local choice.
            // Only this field changes, so unsaved login edits stay in the draft.
            core::Settings next = app_.settings;
            next.autoBackup = !next.autoBackup;
            if (auto saved = platform::saveSettings(next); !saved) {
                message_ = "Could not save the settings: " + saved.error().detail;
                messageColor_ = kError;
                break;
            }
            app_.settings.autoBackup = next.autoBackup;
            draft_.autoBackup = next.autoBackup;
            message_ = next.autoBackup ? "Back up on start: on." : "Back up on start: off. Use Sync now in the menu.";
            messageColor_ = kOk;
            break;
        }
        default: break;
    }
}

ScreenId SettingsScreen::update(u32 down, const touchPosition* touch) {
    if (phase_ == Phase::Checking) {
        if (!app_.worker.running()) {
            finishCheck();
        } else if (down & KEY_B) {
            app_.cancel.requested = true;
        }
        return ScreenId::Settings;
    }
    if (phase_ == Phase::ConfirmSignOut) {
        if ((down & KEY_A) || (touch != nullptr && Ui::hit(confirm_, *touch))) {
            if (auto cleared = platform::clearLogin(app_.settings); !cleared) {
                message_ = "Could not sign out: " + cleared.error().detail;
                messageColor_ = kError;
                phase_ = Phase::Edit;
                return ScreenId::Settings;
            }
            app_.settings.user.clear();
            app_.settings.password.clear();
            return ScreenId::Setup;
        }
        if ((down & KEY_B) || (touch != nullptr && Ui::hit(back_, *touch))) {
            phase_ = Phase::Edit;
        }
        return ScreenId::Settings;
    }

    if (down & KEY_B) {
        return ScreenId::Menu;  // unsaved changes are dropped
    }
    // 2×3 grid, then the checkbox row across the full width.
    if ((down & (KEY_DLEFT | KEY_DRIGHT)) && focus_ != kAutoBackup) {
        focus_ ^= 1;
    }
    if (down & KEY_DDOWN) {
        focus_ = focus_ == kAutoBackup ? kEmail : focus_ >= kSave ? kAutoBackup : focus_ + 2;
    }
    if (down & KEY_DUP) {
        focus_ = focus_ == kAutoBackup ? kSave : focus_ < kFolder ? kAutoBackup : focus_ - 2;
    }
    if (down & KEY_A) {
        press(focus_);
    } else if (touch != nullptr) {
        for (int i = 0; i < kActionCount; ++i) {
            if (Ui::hit(buttons_[static_cast<size_t>(i)], *touch)) {
                focus_ = i;
                press(i);
                break;
            }
        }
    }
    return ScreenId::Settings;
}

void SettingsScreen::draw() {
    Ui& ui = app_.ui;
    if (phase_ == Phase::ConfirmSignOut) {
        ui.centered(GFX_TOP, 90.0f, "Sign out of Koofr?", kWhite);
        ui.centered(GFX_TOP, 124.0f, "The saved email and app password will be deleted.", kText, 0.45f);
        ui.button(confirm_);
        ui.button(back_);
        return;
    }

    ui.bar(GFX_TOP, "Settings", "");
    ui.text(GFX_TOP, 10.0f, 34.0f, "Email", kMuted);
    ui.text(GFX_TOP, 120.0f, 34.0f, ui.fit(draft_.user, kTopWidth - 130.0f), kText);
    ui.text(GFX_TOP, 10.0f, 52.0f, "App password", kMuted);
    ui.text(GFX_TOP, 120.0f, 52.0f, draft_.password.empty() ? "not set" : "●●●●●●●●",
            draft_.password.empty() ? kMuted : kText, 0.4f);
    ui.text(GFX_TOP, 10.0f, 70.0f, "Cloud folder", kMuted);
    ui.text(GFX_TOP, 120.0f, 70.0f, ui.fit(draft_.root, kTopWidth - 130.0f), kText);
    ui.text(GFX_TOP, 120.0f, 86.0f, "Existing backups stay in the old folder.", kMuted, 0.45f);
    const bool changed = draft_.user != app_.settings.user || draft_.password != app_.settings.password ||
                         draft_.root != app_.settings.root;
    if (changed && phase_ == Phase::Edit && message_.empty()) {
        ui.text(GFX_TOP, 10.0f, 116.0f, "Unsaved changes: Save checks them with Koofr first.", kCurrent, 0.45f);
    }
    if (!message_.empty()) {
        ui.text(GFX_TOP, 10.0f, 116.0f, message_, messageColor_, 0.5f, kTopWidth - 20.0f);
    }

    for (int i = 0; i < kAutoBackup; ++i) {
        Button b = buttons_[static_cast<size_t>(i)];
        b.disabled = phase_ == Phase::Checking;
        ui.button(b, phase_ == Phase::Edit && i == focus_);
    }
    const Button& row = buttons_[kAutoBackup];
    if (phase_ == Phase::Edit && focus_ == kAutoBackup) {
        ui.rect(GFX_BOTTOM, row.x - 3.0f, row.y - 3.0f, row.w + 6.0f, row.h + 6.0f, kCurrent);
        ui.rect(GFX_BOTTOM, row.x - 1.0f, row.y - 1.0f, row.w + 2.0f, row.h + 2.0f, kBackground);
    }
    ui.checkbox(GFX_BOTTOM, row.x + 6.0f, row.y + 7.0f, 14.0f, app_.settings.autoBackup);
    ui.text(GFX_BOTTOM, row.x + 28.0f, row.y + 6.0f, row.label, phase_ == Phase::Checking ? kMuted : kText);
    footHint(ui, phase_ == Phase::Checking ? "B — cancel" : "B — back (unsaved changes are dropped)");
}

}  // namespace ui
