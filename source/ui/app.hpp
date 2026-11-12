// What the screens share: the drawing helper, the settings in use, the
// background worker and its cancel flag.
#pragma once

#include <optional>
#include <string_view>

#include "core/cancel.hpp"
#include "core/cloud_store.hpp"
#include "core/settings.hpp"
#include "core/webdav_client.hpp"
#include "platform/curl_transport.hpp"
#include "ui/ui.hpp"
#include "ui/worker.hpp"

namespace ui {

enum class ScreenId { Setup, Sync, Menu, Restore, Delete, Settings, Exit };

struct App {
    explicit App(Ui& u) : ui(u) {}
    Ui& ui;
    core::Settings settings;
    Worker worker;
    core::CancelToken cancel;
};

// A Koofr connection for one worker job.
struct Koofr {
    Koofr(const core::Settings& s, const core::CancelToken* cancel)
        : transport(s.user, s.password, cancel), dav(transport, core::kKoofrDavUrl), cloud(dav, s.root) {}
    platform::CurlTransport transport;
    core::WebDavClient dav;
    core::CloudStore cloud;
};

// Koofr accepts the login if it can list the cloud folder; NotFound is fine,
// the first backup creates the folder.
std::optional<core::Error> checkLogin(const core::Settings& s, const core::CancelToken* cancel);

// "START — exit" style hint at the foot of the bottom screen.
void footHint(Ui& ui, std::string_view text);

}  // namespace ui
