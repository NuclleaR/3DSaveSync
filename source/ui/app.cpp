#include "ui/app.hpp"

namespace ui {

std::optional<core::Error> checkLogin(const core::Settings& s, const core::CancelToken* cancel) {
    Koofr koofr(s, cancel);
    auto listed = koofr.dav.list(s.root);
    if (listed || listed.error().kind == core::ErrorKind::NotFound) {
        return std::nullopt;
    }
    return listed.error();
}

void footHint(Ui& ui, std::string_view text) {
    ui.centered(GFX_BOTTOM, 214.0f, text, kMuted, 0.45f);
}

}  // namespace ui
