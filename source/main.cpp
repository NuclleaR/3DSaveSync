#include <3ds.h>

#include <optional>
#include <string>

#include "core/settings.hpp"
#include "core/webdav_client.hpp"
#include "platform/curl_transport.hpp"
#include "platform/secure_store.hpp"
#include "ui/setup_screen.hpp"
#include "ui/ui.hpp"

namespace {

struct Status {
    std::string text;
    u32 color;
    std::string bar;  // right side of the top bar
    u32 barColor;
};

void draw(ui::Ui& ui, const Status& status) {
    ui.beginFrame();
    ui.topBar(status.bar, status.barColor);
    ui.text(GFX_TOP, 10.0f, 34.0f, status.text, status.color, 0.5f, ui::kTopWidth - 20.0f);
    const char* hint = "START - exit";
    ui.text(GFX_BOTTOM, (ui::kBottomWidth - ui.textWidth(hint, 0.45f)) / 2.0f, 210.0f, hint, ui::kMuted, 0.45f);
    ui.endFrame();
}

Status checkConnection(const core::Settings& s) {
    platform::CurlTransport transport(s.user, s.password, nullptr);
    core::WebDavClient dav(transport, core::kKoofrDavUrl);
    auto listed = dav.list(s.root);
    if (listed || listed.error().kind == core::ErrorKind::NotFound) {
        return {"Connected to Koofr", ui::kOk, "Koofr", ui::kOk};
    }
    return {core::describe(listed.error()), ui::kError, "Koofr", ui::kError};
}

// Settings from the save archive, or from the setup screen on first launch.
// nullopt with status set: cannot continue; nullopt without: owner exited.
std::optional<core::Settings> obtainSettings(ui::Ui& ui, std::optional<Status>& status) {
    auto loaded = platform::loadSettings();
    if (!loaded) {
        status = Status{"Could not read the settings: " + loaded.error().detail, ui::kError, "", ui::kMuted};
        return std::nullopt;
    }
    if (*loaded) {
        return **loaded;
    }
    return ui::runSetup(ui);
}

}  // namespace

int main() {
    romfsInit();
    ui::Ui ui;
    if (!ui.init()) {
        romfsExit();
        return 1;
    }

    std::optional<Status> status;
    if (!platform::networkInit()) {
        status = Status{"Could not start the network service.", ui::kError, "", ui::kMuted};
    } else if (auto settings = obtainSettings(ui, status)) {
        draw(ui, {"Connecting to Koofr...", ui::kCurrent, "Koofr", ui::kMuted});
        status = checkConnection(*settings);
    }

    if (status) {
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_START) {
                break;
            }
            draw(ui, *status);
        }
    }

    platform::networkExit();
    romfsExit();
    return 0;
}
