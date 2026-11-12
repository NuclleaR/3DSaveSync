#include "ui/list_view.hpp"

#include <algorithm>

namespace ui {

ListView::ListView(gfxScreen_t screen, float y, int visibleRows) : screen_(screen), y_(y), visible_(visibleRows) {}

void ListView::setRows(std::vector<ListRow> rows) {
    rows_ = std::move(rows);
    select(selected_);
}

void ListView::select(int i) {
    selected_ = rows_.empty() ? 0 : std::clamp(i, 0, static_cast<int>(rows_.size()) - 1);
    keepVisible();
}

bool ListView::handle(u32 down) {
    const int before = selected_;
    if (down & KEY_DDOWN) {
        select(selected_ + 1);
    }
    if (down & KEY_DUP) {
        select(selected_ - 1);
    }
    if (down & KEY_R) {
        select(selected_ + visible_);
    }
    if (down & KEY_L) {
        select(selected_ - visible_);
    }
    return selected_ != before;
}

std::optional<int> ListView::rowAt(const touchPosition& p) const {
    if (screen_ != GFX_BOTTOM || p.py < y_) {
        return std::nullopt;
    }
    const int row = first_ + static_cast<int>((static_cast<float>(p.py) - y_) / kRowHeight);
    if (row >= first_ + visible_ || row >= static_cast<int>(rows_.size())) {
        return std::nullopt;
    }
    return row;
}

void ListView::keepVisible() {
    first_ = std::clamp(first_, selected_ - visible_ + 1, selected_);
    first_ = std::max(0, first_);
}

void ListView::draw(Ui& ui, bool focused) const {
    const float width = screen_ == GFX_TOP ? kTopWidth : kBottomWidth;
    const int last = std::min(static_cast<int>(rows_.size()), first_ + visible_);
    for (int i = first_; i < last; ++i) {
        const ListRow& r = rows_[i];
        const float y = y_ + static_cast<float>(i - first_) * kRowHeight;
        const bool sel = i == selected_;
        if (sel) {
            ui.rect(screen_, 0.0f, y - 1.0f, width, kRowHeight, focused ? kAccent : kButtonBorder);
        }
        float x = 10.0f;
        if (r.checkbox) {
            ui.checkbox(screen_, x, y + 2.0f, 10.0f, r.checked);
            x += 16.0f;
        }
        const float rightWidth = r.right.empty() ? 0.0f : ui.textWidth(r.right) + 12.0f;
        ui.text(screen_, x, y, ui.fit(r.left, width - 10.0f - x - rightWidth), sel ? kWhite : r.leftColor);
        if (!r.right.empty()) {
            ui.text(screen_, width - 10.0f - ui.textWidth(r.right), y, r.right, sel ? kWhite : r.rightColor);
        }
    }
    if (static_cast<int>(rows_.size()) > visible_) {
        // Scroll position: a thin bar at the right edge.
        const float trackH = static_cast<float>(visible_) * kRowHeight;
        const float thumbH = std::max(6.0f, trackH * static_cast<float>(visible_) / static_cast<float>(rows_.size()));
        const float thumbY = y_ + (trackH - thumbH) * static_cast<float>(first_) /
                                      static_cast<float>(rows_.size() - static_cast<size_t>(visible_));
        ui.rect(screen_, width - 3.0f, y_ - 1.0f, 2.0f, trackH, kLine);
        ui.rect(screen_, width - 3.0f, thumbY - 1.0f, 2.0f, thumbH, kMuted);
    }
}

}  // namespace ui
