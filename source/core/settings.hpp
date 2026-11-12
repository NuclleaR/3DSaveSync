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
    bool autoBackup = true;  // back up every game when the app starts
};

// "user=…\npassword=…\nroot=…\nautobackup=1|0\n"
std::string serializeSettings(const Settings& s);

// Splits each line on its first '='. Missing root → kDefaultRoot; missing
// autobackup → on (files written before the option existed); only
// "autobackup=0" turns it off. A missing or empty user or password → nullopt
// (no login: first launch or signed out).
std::optional<Settings> parseSettings(std::string_view text);

// What survives Sign out, read even without a login: the cloud folder and the
// auto backup choice; user and password are empty.
Settings parseKept(std::string_view text);

// Leading '/', no trailing '/'; empty (or just "/") → kDefaultRoot.
std::string normalizeRoot(std::string_view root);

}  // namespace core
