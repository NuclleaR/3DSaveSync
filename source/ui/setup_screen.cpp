#include "ui/setup_screen.hpp"

#include <array>
#include <string>

#include "platform/keyboard.hpp"
#include "platform/secure_store.hpp"
#include "ui/app.hpp"

namespace ui {

namespace {

enum Field { kEmail, kPassword, kConnect, kFieldCount };

struct Status {
    std::string text;
    u32 color = kMuted;
};

void draw(Ui& ui, const core::Settings& s, const std::array<Button, kFieldCount>& buttons, int focus,
          const Status& status) {
    ui.beginFrame();
    ui.topBar("Koofr", kMuted);
    ui.text(GFX_TOP, 10.0f, 34.0f, "Koofr login", kText, 0.6f);
    ui.text(GFX_TOP, 10.0f, 62.0f, "Email", kMuted);
    ui.text(GFX_TOP, 120.0f, 62.0f, s.user.empty() ? "not set" : s.user, s.user.empty() ? kMuted : kText);
    ui.text(GFX_TOP, 10.0f, 80.0f, "App password", kMuted);
    ui.text(GFX_TOP, 120.0f, 80.0f, s.password.empty() ? "not set" : "set", s.password.empty() ? kMuted : kText);
    ui.text(GFX_TOP, 10.0f, 98.0f, "Cloud folder", kMuted);
    ui.text(GFX_TOP, 120.0f, 98.0f, s.root, kText);
    ui.text(GFX_TOP, 10.0f, 128.0f, status.text, status.color, 0.5f, kTopWidth - 20.0f);
    ui.text(GFX_TOP, 10.0f, 190.0f,
            "Use a Koofr app password (Koofr: Preferences > Password > App passwords). "
            "The account password does not work here.",
            kMuted, 0.45f, kTopWidth - 20.0f);
    for (int i = 0; i < kFieldCount; ++i) {
        ui.button(buttons[i], i == focus);
    }
    footHint(ui, "START — exit");
    ui.endFrame();
}

}  // namespace

std::optional<core::Settings> runSetup(Ui& ui, core::Settings kept) {
    core::Settings s = std::move(kept);
    s.user.clear();
    s.password.clear();
    const std::array<Button, kFieldCount> buttons{{
        {10.0f, 40.0f, 145.0f, 40.0f, "Email"},
        {165.0f, 40.0f, 145.0f, 40.0f, "App password"},
        {10.0f, 100.0f, 300.0f, 40.0f, "Connect", true},
    }};
    int focus = kEmail;
    Status status{"Enter your Koofr email and app password, then Connect."};

    while (aptMainLoop()) {
        hidScanInput();
        const u32 down = hidKeysDown();
        if (down & KEY_START) {
            return std::nullopt;
        }
        if (down & (KEY_DUP | KEY_DLEFT)) {
            focus = (focus + kFieldCount - 1) % kFieldCount;
        }
        if (down & (KEY_DDOWN | KEY_DRIGHT)) {
            focus = (focus + 1) % kFieldCount;
        }
        int pressed = -1;
        if (down & KEY_A) {
            pressed = focus;
        }
        if (down & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            for (int i = 0; i < kFieldCount; ++i) {
                if (Ui::hit(buttons[i], touch)) {
                    pressed = focus = i;
                }
            }
        }

        if (pressed == kEmail) {
            if (auto text = platform::askText("Koofr email", s.user, false)) {
                s.user = *text;
            }
        } else if (pressed == kPassword) {
            if (auto text = platform::askText("Koofr app password", "", true)) {
                s.password = *text;
            }
        } else if (pressed == kConnect) {
            if (s.user.empty() || s.password.empty()) {
                status = {"Enter the email and the app password first.", kError};
            } else {
                draw(ui, s, buttons, focus, {"Connecting to Koofr...", kCurrent});
                if (auto err = checkLogin(s, nullptr)) {
                    status = {core::describe(*err), kError};
                } else if (auto saved = platform::saveSettings(s); !saved) {
                    status = {"Could not save the settings: " + saved.error().detail, kError};
                } else {
                    return s;
                }
            }
        }
        draw(ui, s, buttons, focus, status);
    }
    return std::nullopt;
}

}  // namespace ui
