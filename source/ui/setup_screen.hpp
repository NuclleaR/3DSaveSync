#pragma once

#include <optional>
#include <string>

#include "core/settings.hpp"
#include "ui/ui.hpp"

namespace ui {

// Koofr login, on first launch and after Sign out. kept holds the cloud
// folder and auto backup choice to keep. Returns the settings once Koofr
// accepted them and they were saved; nullopt when the owner exits with START.
// Runs its own loop; the network check blocks the screen while it runs.
std::optional<core::Settings> runSetup(Ui& ui, core::Settings kept);

}  // namespace ui
