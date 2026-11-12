#include "ui/game_list.hpp"

#include <algorithm>

#include "core/checkpoint.hpp"

namespace ui {

std::expected<std::vector<CloudTitle>, core::Error> loadCloudTitles(
    core::CloudStore& cloud, const std::function<void(size_t, size_t)>& progress, const core::CancelToken& cancel) {
    auto folders = cloud.listTitles();
    if (!folders) {
        return std::unexpected(folders.error());
    }
    std::vector<CloudTitle> out;
    for (size_t i = 0; i < folders->size(); ++i) {
        if (cancel.cancelled()) {
            return std::unexpected(core::Error{core::ErrorKind::Cancelled, ""});
        }
        const std::string& folder = (*folders)[i];
        auto versions = cloud.listVersions(folder);
        if (!versions) {
            return std::unexpected(versions.error());
        }
        out.push_back({folder, core::displayNameFromFolder(folder), std::move(*versions)});
        progress(i + 1, folders->size());
    }
    std::sort(out.begin(), out.end(), [](const CloudTitle& a, const CloudTitle& b) { return a.name < b.name; });
    return out;
}

std::string versionCount(size_t n) {
    return std::to_string(n) + (n == 1 ? " version" : " versions");
}

GameListView::GameListView() : list_(GFX_TOP, 30.0f, 11) {}

void GameListView::set(std::vector<CloudTitle> titles) {
    titles_ = std::move(titles);
    refresh();
}

CloudTitle* GameListView::current() {
    if (titles_.empty()) {
        return nullptr;
    }
    return &titles_[static_cast<size_t>(list_.selected())];
}

bool GameListView::handle(u32 down) {
    return list_.handle(down);
}

void GameListView::refresh() {
    std::vector<ListRow> rows;
    rows.reserve(titles_.size());
    for (const auto& t : titles_) {
        rows.push_back({t.name, versionCount(t.versions.size())});
    }
    list_.setRows(std::move(rows));
}

void GameListView::draw(Ui& ui, std::string_view heading, bool focused) const {
    ui.bar(GFX_TOP, heading, std::to_string(titles_.size()) + (titles_.size() == 1 ? " game" : " games"));
    list_.draw(ui, focused);
}

}  // namespace ui
