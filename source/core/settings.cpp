#include "core/settings.hpp"

namespace core {

std::string serializeSettings(const Settings& s) {
    return "user=" + s.user + "\npassword=" + s.password + "\nroot=" + s.root + "\n";
}

std::optional<Settings> parseSettings(std::string_view text) {
    std::optional<std::string> user;
    std::optional<std::string> password;
    std::optional<std::string> root;
    while (!text.empty()) {
        const auto nl = text.find('\n');
        const std::string_view line = text.substr(0, nl);
        text.remove_prefix(nl == std::string_view::npos ? text.size() : nl + 1);
        const auto eq = line.find('=');
        if (eq == std::string_view::npos) {
            continue;
        }
        const std::string_view key = line.substr(0, eq);
        std::string value(line.substr(eq + 1));
        if (key == "user") {
            user = std::move(value);
        } else if (key == "password") {
            password = std::move(value);
        } else if (key == "root") {
            root = std::move(value);
        }
    }
    if (!user || !password) {
        return std::nullopt;
    }
    return Settings{std::move(*user), std::move(*password), root ? normalizeRoot(*root) : std::string(kDefaultRoot)};
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
