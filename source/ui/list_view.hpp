// A scrolling list of rows with a selection, on either screen.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ui/ui.hpp"

namespace ui {

struct ListRow {
    std::string left;
    std::string right;
    u32 leftColor = kText;
    u32 rightColor = kMuted;
    bool checkbox = false;  // draws a box before the text
    bool checked = false;
};

class ListView {
public:
    static constexpr float kRowHeight = 16.0f;

    ListView(gfxScreen_t screen, float y, int visibleRows);

    void setRows(std::vector<ListRow> rows);  // keeps the selection in range
    std::vector<ListRow>& rows() { return rows_; }
    const std::vector<ListRow>& rows() const { return rows_; }
    bool empty() const { return rows_.empty(); }
    int selected() const { return selected_; }
    void select(int i);
    // Up/Down move the selection; true if it moved.
    bool handle(u32 down);
    // The row under a touch (bottom screen lists only).
    std::optional<int> rowAt(const touchPosition& p) const;
    // focused: the D-pad currently drives this list.
    void draw(Ui& ui, bool focused) const;
    float bottom() const { return y_ + static_cast<float>(visible_) * kRowHeight; }

private:
    void keepVisible();

    gfxScreen_t screen_;
    float y_;
    int visible_;
    std::vector<ListRow> rows_;
    int selected_ = 0;
    int first_ = 0;
};

}  // namespace ui
