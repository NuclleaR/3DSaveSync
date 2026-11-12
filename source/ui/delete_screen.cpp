#include "ui/delete_screen.hpp"

#include <algorithm>
#include <mutex>

#include "core/checkpoint.hpp"

namespace ui {

namespace {

std::string deleteLabel(size_t n) {
    return "Delete " + versionCount(n);
}

}  // namespace

DeleteScreen::DeleteScreen(App& app)
    : app_(app),
      versions_(GFX_BOTTOM, 30.0f, 8),
      delete_{10.0f, 164.0f, 300.0f, 32.0f, "", false, true},
      confirm_{10.0f, 80.0f, 145.0f, 36.0f, "A — Delete", false, true},
      back_{165.0f, 80.0f, 145.0f, 36.0f, "B — Cancel"} {}

void DeleteScreen::enter() {
    phase_ = Phase::Loading;
    leaving_ = false;
    onVersions_ = false;
    onButton_ = false;
    message_.clear();
    games_.set({});
    versions_.setRows({});
    {
        std::lock_guard lock(mutex_);
        shared_ = Shared{};
    }
    app_.cancel.requested = false;
    const core::Settings s = app_.settings;
    const bool started = app_.worker.start([this, s] {
        Koofr koofr(s, &app_.cancel);
        auto titles = loadCloudTitles(
            koofr.cloud,
            [this](size_t done, size_t total) {
                std::lock_guard lock(mutex_);
                shared_.done = done;
                shared_.total = total;
            },
            app_.cancel);
        std::lock_guard lock(mutex_);
        if (titles) {
            shared_.titles = std::move(*titles);
        } else {
            shared_.error = titles.error();
        }
    });
    if (!started) {
        phase_ = Phase::Pick;
        message_ = "Could not start the background task.";
        messageColor_ = kError;
    }
}

void DeleteScreen::showVersions() {
    std::vector<ListRow> rows;
    if (const CloudTitle* t = games_.current()) {
        for (size_t i = 0; i < t->versions.size(); ++i) {
            ListRow r{core::versionLabel(t->versions[i]), i == 0 ? "latest" : ""};
            r.checkbox = true;
            rows.push_back(std::move(r));
        }
    }
    versions_.setRows(std::move(rows));
    versions_.select(0);
    onButton_ = false;
}

size_t DeleteScreen::selectedCount() const {
    const auto& rows = versions_.rows();
    return static_cast<size_t>(std::count_if(rows.begin(), rows.end(), [](const ListRow& r) { return r.checked; }));
}

void DeleteScreen::startDelete() {
    const CloudTitle* t = games_.current();
    if (t == nullptr) {
        return;
    }
    std::vector<std::string> chosen;
    for (size_t i = 0; i < versions_.rows().size(); ++i) {
        if (versions_.rows()[i].checked) {
            chosen.push_back(t->versions[i]);
        }
    }
    if (chosen.empty()) {
        return;
    }
    {
        std::lock_guard lock(mutex_);
        shared_.done = 0;
        shared_.total = chosen.size();
        shared_.versionsAfter.reset();
        shared_.failed = 0;
        shared_.error.reset();
    }
    // Deleting is not cancellable: a delete stopped half way would leave a
    // version without its files.
    app_.cancel.requested = false;
    const core::Settings s = app_.settings;
    const std::string folder = t->folder;
    const bool started = app_.worker.start([this, s, folder, chosen] {
        Koofr koofr(s, nullptr);
        size_t failed = 0;
        std::optional<core::Error> firstError;
        for (const auto& v : chosen) {
            if (auto r = koofr.cloud.deleteVersion(folder, v); !r) {
                ++failed;
                if (!firstError) {
                    firstError = r.error();
                }
            }
            std::lock_guard lock(mutex_);
            shared_.done += 1;
        }
        auto after = koofr.cloud.listVersions(folder);
        std::lock_guard lock(mutex_);
        shared_.failed = failed;
        shared_.error = firstError;
        if (after) {
            shared_.versionsAfter = std::move(*after);
        } else if (!shared_.error) {
            shared_.error = after.error();
        }
    });
    if (!started) {
        phase_ = Phase::Pick;
        message_ = "Could not start the background task.";
        messageColor_ = kError;
        return;
    }
    deleting_ = chosen.size();
    message_.clear();
    phase_ = Phase::Working;
}

void DeleteScreen::finishJob() {
    std::lock_guard lock(mutex_);
    if (phase_ == Phase::Loading) {
        if (shared_.titles) {
            games_.set(std::move(*shared_.titles));
            shared_.titles.reset();
            showVersions();
            if (games_.empty()) {
                message_ = "No backups in Koofr yet.";
                messageColor_ = kMuted;
            }
        } else if (shared_.error) {
            message_ = core::describe(*shared_.error);
            messageColor_ = kError;
        }
    } else if (phase_ == Phase::Working) {
        if (CloudTitle* t = games_.current(); t != nullptr && shared_.versionsAfter) {
            t->versions = std::move(*shared_.versionsAfter);
            shared_.versionsAfter.reset();
            games_.refresh();
            showVersions();
        }
        if (shared_.failed == 0 && !shared_.error) {
            message_ = "Deleted " + versionCount(deleting_) + ".";
            messageColor_ = kOk;
        } else {
            message_ = shared_.failed > 0 ? "Could not delete " + std::to_string(shared_.failed) + " of " +
                                                versionCount(deleting_) + ": "
                                          : "";
            message_ += shared_.error ? core::describe(*shared_.error) : "";
            messageColor_ = kError;
        }
    }
    phase_ = Phase::Pick;
}

ScreenId DeleteScreen::update(u32 down, const touchPosition* touch) {
    if (phase_ == Phase::Loading || phase_ == Phase::Working) {
        if (!app_.worker.running()) {
            finishJob();
            return leaving_ ? ScreenId::Menu : ScreenId::Delete;
        }
        if (phase_ == Phase::Loading && (down & KEY_B)) {
            app_.cancel.requested = true;
            leaving_ = true;
        }
        return ScreenId::Delete;
    }

    if (phase_ == Phase::Confirm) {
        if ((down & KEY_A) || (touch != nullptr && Ui::hit(confirm_, *touch))) {
            startDelete();
        } else if ((down & KEY_B) || (touch != nullptr && Ui::hit(back_, *touch))) {
            phase_ = Phase::Pick;
        }
        return ScreenId::Delete;
    }

    Button del = delete_;
    del.disabled = selectedCount() == 0;
    if (touch != nullptr) {
        if (auto row = versions_.rowAt(*touch)) {
            versions_.select(*row);
            versions_.rows()[static_cast<size_t>(*row)].checked ^= true;
            onVersions_ = true;
            onButton_ = false;
        } else if (Ui::hit(del, *touch)) {
            phase_ = Phase::Confirm;
        }
        return ScreenId::Delete;
    }
    if (!onVersions_) {
        if (down & KEY_B) {
            return ScreenId::Menu;
        }
        if (games_.handle(down)) {
            showVersions();
        }
        if ((down & (KEY_A | KEY_DRIGHT)) && !versions_.empty()) {
            onVersions_ = true;
        }
        return ScreenId::Delete;
    }
    if (down & (KEY_B | KEY_DLEFT)) {
        onVersions_ = false;
        onButton_ = false;
        return ScreenId::Delete;
    }
    if (onButton_) {
        if (down & KEY_DUP) {
            onButton_ = false;
        } else if ((down & KEY_A) && !del.disabled) {
            phase_ = Phase::Confirm;
        }
        return ScreenId::Delete;
    }
    const bool atLast = versions_.selected() == static_cast<int>(versions_.rows().size()) - 1;
    if ((down & KEY_DDOWN) && atLast) {
        onButton_ = true;
        return ScreenId::Delete;
    }
    versions_.handle(down);
    if ((down & KEY_A) && !versions_.empty()) {
        versions_.rows()[static_cast<size_t>(versions_.selected())].checked ^= true;
    }
    return ScreenId::Delete;
}

void DeleteScreen::draw() {
    Ui& ui = app_.ui;
    if (phase_ == Phase::Loading) {
        size_t done = 0;
        size_t total = 0;
        {
            std::lock_guard lock(mutex_);
            done = shared_.done;
            total = shared_.total;
        }
        ui.bar(GFX_TOP, "Delete · pick a game", "");
        std::string line = "Loading backups from Koofr…";
        if (total > 0) {
            line += " " + std::to_string(done) + " / " + std::to_string(total);
        }
        ui.text(GFX_TOP, 10.0f, 30.0f, line, kCurrent);
        footHint(ui, "B — back");
        return;
    }

    const CloudTitle* t = games_.current();
    const size_t n = selectedCount();
    if (phase_ == Phase::Confirm) {
        ui.centered(GFX_TOP, 80.0f, deleteLabel(n) + " of", kText);
        ui.centered(GFX_TOP, 98.0f, (t != nullptr ? t->name : "") + " from Koofr?", kWhite);
        ui.centered(GFX_TOP, 132.0f, "This cannot be undone.", kError);
        ui.button(confirm_);
        ui.button(back_);
        return;
    }

    games_.draw(ui, "Delete · pick a game", phase_ == Phase::Pick && !onVersions_);
    if (!message_.empty()) {
        ui.text(GFX_TOP, 10.0f, GameListView::kBottom + 2.0f, message_, messageColor_, 0.45f, kTopWidth - 20.0f);
    }
    if (phase_ == Phase::Working) {
        size_t done = 0;
        {
            std::lock_guard lock(mutex_);
            done = shared_.done;
        }
        ui.text(GFX_BOTTOM, 10.0f, 40.0f,
                "Deleting " + std::to_string(done) + " / " + std::to_string(deleting_) + "…", kCurrent);
        return;
    }

    ui.bar(GFX_BOTTOM, t != nullptr ? t->name : "", n > 0 ? std::to_string(n) + " selected" : "");
    versions_.draw(ui, onVersions_ && !onButton_);
    Button del = delete_;
    del.label = deleteLabel(n);
    del.disabled = n == 0;
    ui.button(del, onButton_);
    footHint(ui, onVersions_ ? "A — tick   B — games" : "A — versions   B — menu");
}

}  // namespace ui
