#pragma once

#include <optional>

#include "core/settings.hpp"
#include "ui/ui.hpp"

namespace ui {

// First-launch Koofr login. Returns the settings once Koofr accepted them and
// they were saved; nullopt when the owner exits with START.
std::optional<core::Settings> runSetup(Ui& ui);

}  // namespace ui
