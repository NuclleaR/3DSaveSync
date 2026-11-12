// The games that have backups in the cloud (installed or not), shown on the
// top screen by Restore and Delete.
#pragma once

#include <cstddef>
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "core/cancel.hpp"
#include "core/cloud_store.hpp"
#include "ui/list_view.hpp"

namespace ui {

struct CloudTitle {
    std::string folder;                 // Checkpoint title folder
    std::string name;                   // displayNameFromFolder(folder)
    std::vector<std::string> versions;  // complete versions, newest first
};

// Every title under <root>/manifests with its versions, sorted by name.
// progress(done, total) is called after each title.
std::expected<std::vector<CloudTitle>, core::Error> loadCloudTitles(
    core::CloudStore& cloud, const std::function<void(size_t, size_t)>& progress, const core::CancelToken& cancel);

// "1 version", "5 versions".
std::string versionCount(size_t n);

class GameListView {
public:
    GameListView();

    void set(std::vector<CloudTitle> titles);
    bool empty() const { return titles_.empty(); }
    CloudTitle* current();
    // Up/Down/L/R.
    bool handle(u32 down);
    // After a title's versions changed.
    void refresh();
    // Top screen: the bar with heading and "N games", then the list.
    void draw(Ui& ui, std::string_view heading, bool focused) const;

    static constexpr float kBottom = 30.0f + 11 * ListView::kRowHeight;

private:
    std::vector<CloudTitle> titles_;
    ListView list_;
};

}  // namespace ui
