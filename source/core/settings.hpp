#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace core {

inline constexpr const char* kDefaultRoot = "/3DSaveSync";
inline constexpr const char* kKoofrDavUrl = "https://app.koofr.net/dav/Koofr";

struct Settings {
    std::string user;      // Koofr account email
    std::string password;  // Koofr app password
    std::string root = kDefaultRoot;
};

// "user=…\npassword=…\nroot=…\n"
std::string serializeSettings(const Settings& s);

// Splits each line on its first '='. Missing root → kDefaultRoot; missing
// user or password → nullopt.
std::optional<Settings> parseSettings(std::string_view text);

// Leading '/', no trailing '/'; empty (or just "/") → kDefaultRoot.
std::string normalizeRoot(std::string_view root);

}  // namespace core
