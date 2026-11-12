#include <3ds.h>

#include <optional>
#include <string>

#include "core/settings.hpp"
#include "platform/curl_transport.hpp"
#include "platform/secure_store.hpp"
#include "ui/app.hpp"
#include "ui/delete_screen.hpp"
#include "ui/menu_screen.hpp"
#include "ui/restore_screen.hpp"
#include "ui/settings_screen.hpp"
#include "ui/setup_screen.hpp"
#include "ui/sync_screen.hpp"
#include "ui/ui.hpp"

// Launch: settings (or setup on first launch), then the backup runs at once;
// the menu leads to Restore, Delete versions, Sync again and Settings.

namespace {

// Shown when the app cannot start at all; START exits.
void fatalScreen(ui::Ui& ui, const std::string& text) {
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) {
            break;
        }
        ui.beginFrame();
        ui.topBar("", ui::kMuted);
        ui.text(GFX_TOP, 10.0f, 30.0f, text, ui::kError, 0.5f, ui::kTopWidth - 20.0f);
        ui::footHint(ui, "START — exit");
        ui.endFrame();
    }
}

}  // namespace

int main() {
    romfsInit();
    ui::Ui ui;
    if (!ui.init()) {
        romfsExit();
        return 1;
    }
    if (!platform::networkInit()) {
        fatalScreen(ui, "Could not start the network service.");
        romfsExit();
        return 0;
    }

    ui::App app(ui);
    auto loaded = platform::loadSettings();
    if (!loaded) {
        fatalScreen(ui, "Could not read the settings: " + loaded.error().detail);
        platform::networkExit();
        romfsExit();
        return 0;
    }
    bool run = true;
    if (*loaded) {
        app.settings = **loaded;
    } else if (auto entered = ui::runSetup(ui, platform::loadKept())) {
        app.settings = *entered;
    } else {
        run = false;
    }

    ui::SyncScreen sync(app);
    ui::MenuScreen menu(app, sync);
    ui::RestoreScreen restore(app);
    ui::DeleteScreen remove(app);
    ui::SettingsScreen settings(app);

    // Back up on start unless the owner turned it off in Settings.
    ui::ScreenId screen = app.settings.autoBackup ? ui::ScreenId::Sync : ui::ScreenId::Menu;
    if (run && screen == ui::ScreenId::Sync) {
        sync.start();
    }
    while (run && aptMainLoop()) {
        hidScanInput();
        const u32 down = hidKeysDown();
        touchPosition touch{};
        const touchPosition* touched = nullptr;
        if (down & KEY_TOUCH) {
            hidTouchRead(&touch);
            touched = &touch;
        }
        if ((down & KEY_START) && screen != ui::ScreenId::Menu) {
            // START leaves from anywhere; a running job is cancelled first.
            break;
        }

        ui::ScreenId next = screen;
        switch (screen) {
            case ui::ScreenId::Sync: next = sync.update(down, touched); break;
            case ui::ScreenId::Menu: next = menu.update(down, touched); break;
            case ui::ScreenId::Restore: next = restore.update(down, touched); break;
            case ui::ScreenId::Delete: next = remove.update(down, touched); break;
            case ui::ScreenId::Settings: next = settings.update(down, touched); break;
            default: break;
        }

        if (next != screen) {
            switch (next) {
                case ui::ScreenId::Sync: sync.start(); break;
                case ui::ScreenId::Restore: restore.enter(); break;
                case ui::ScreenId::Delete: remove.enter(); break;
                case ui::ScreenId::Settings: settings.enter(); break;
                case ui::ScreenId::Setup:
                    // After Sign out: log in again, then continue as on launch.
                    if (auto entered = ui::runSetup(ui, app.settings)) {
                        app.settings = *entered;
                        next = app.settings.autoBackup ? ui::ScreenId::Sync : ui::ScreenId::Menu;
                        if (next == ui::ScreenId::Sync) {
                            sync.start();
                        }
                    } else {
                        next = ui::ScreenId::Exit;
                    }
                    break;
                default: break;
            }
        }
        if (next == ui::ScreenId::Exit) {
            break;
        }
        screen = next;

        ui.beginFrame();
        switch (screen) {
            case ui::ScreenId::Sync: sync.draw(); break;
            case ui::ScreenId::Menu: menu.draw(); break;
            case ui::ScreenId::Restore: restore.draw(); break;
            case ui::ScreenId::Delete: remove.draw(); break;
            case ui::ScreenId::Settings: settings.draw(); break;
            default: break;
        }
        ui.endFrame();
    }

    app.cancel.requested = true;
    app.worker.join();
    platform::networkExit();
    romfsExit();
    return 0;
}
