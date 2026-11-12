#include "core/settings.hpp"

namespace core {

std::string serializeSettings(const Settings& s) {
    return "user=" + s.user + "\npassword=" + s.password + "\nroot=" + s.root + "\nautobackup=" +
           (s.autoBackup ? "1" : "0") + "\n";
}

Settings parseKept(std::string_view text) {
    Settings s;
    while (!text.empty()) {
        const auto nl = text.find('\n');
        const std::string_view line = text.substr(0, nl);
        text.remove_prefix(nl == std::string_view::npos ? text.size() : nl + 1);
        const auto eq = line.find('=');
        if (eq == std::string_view::npos) {
            continue;
        }
        const std::string_view key = line.substr(0, eq);
        const std::string_view value = line.substr(eq + 1);
        if (key == "user") {
            s.user = value;
        } else if (key == "password") {
            s.password = value;
        } else if (key == "root") {
            s.root = normalizeRoot(value);
        } else if (key == "autobackup") {
            s.autoBackup = value != "0";
        }
    }
    return s;
}

std::optional<Settings> parseSettings(std::string_view text) {
    Settings s = parseKept(text);
    if (s.user.empty() || s.password.empty()) {
        return std::nullopt;
    }
    return s;
}

std::string normalizeRoot(std::string_view root) {
    std::string out(root);
    while (!out.empty() && out.back() == '/') {
        out.pop_back();
    }
    if (out.empty()) {
        return kDefaultRoot;
    }
    if (out.front() != '/') {
        out.insert(out.begin(), '/');
    }
    return out;
}

}  // namespace core
