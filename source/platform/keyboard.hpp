#pragma once

#include <optional>
#include <string>

namespace platform {

// The system software keyboard. nullopt when the owner cancels. password
// hides the typed characters.
std::optional<std::string> askText(const char* hint, const std::string& initial, bool password);

}  // namespace platform
