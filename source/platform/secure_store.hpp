// Settings (Koofr email, app password, cloud folder) in the app's own save
// data archive. The console encrypts save data with a console-unique key, so
// the SD card never holds the password in plaintext.
#pragma once

#include <expected>
#include <optional>

#include "core/error.hpp"
#include "core/settings.hpp"

namespace platform {

// nullopt: no settings saved yet (first launch). Errors are ErrorKind::Io.
std::expected<std::optional<core::Settings>, core::Error> loadSettings();
std::expected<void, core::Error> saveSettings(const core::Settings& s);

}  // namespace platform
