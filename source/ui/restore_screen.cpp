#include "ui/restore_screen.hpp"

#include <mutex>

#include "core/checkpoint.hpp"
#include "platform/sd_fs.hpp"
#include "platform/sha256.hpp"

namespace ui {

RestoreScreen::RestoreScreen(App& app)
    : app_(app),
      versions_(GFX_BOTTOM, 30.0f, 8),
      download_{10.0f, 164.0f, 300.0f, 32.0f, "Download to Checkpoint", true},
      cancel_{60.0f, 130.0f, 200.0f, 36.0f, "Cancel"} {}

void RestoreScreen::enter() {
    phase_ = Phase::Loading;
    leaving_ = false;
    onVersions_ = false;
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
                shared_.loaded = done;
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

void RestoreScreen::showVersions() {
    std::vector<ListRow> rows;
    if (const CloudTitle* t = games_.current()) {
        for (size_t i = 0; i < t->versions.size(); ++i) {
            rows.push_back({core::versionLabel(t->versions[i]), i == 0 ? "latest" : ""});
        }
    }
    versions_.setRows(std::move(rows));
    versions_.select(0);
}

void RestoreScreen::startDownload() {
    const CloudTitle* t = games_.current();
    if (t == nullptr || versions_.empty()) {
        return;
    }
    const std::string folder = t->folder;
    const std::string version = t->versions[static_cast<size_t>(versions_.selected())];
    {
        std::lock_guard lock(mutex_);
        shared_.restored.reset();
        shared_.error.reset();
    }
    app_.cancel.requested = false;
    const core::Settings s = app_.settings;
    const bool started = app_.worker.start([this, s, folder, version] {
        Koofr koofr(s, &app_.cancel);
        platform::SdFs fs;
        auto restored = core::restoreVersion(koofr.cloud, fs, platform::sha256Factory(), folder, version, app_.cancel);
        std::lock_guard lock(mutex_);
        if (restored) {
            shared_.restored = std::move(*restored);
        } else {
            shared_.error = restored.error();
        }
    });
    if (!started) {
        message_ = "Could not start the background task.";
        messageColor_ = kError;
        return;
    }
    working_ = "Downloading " + t->name + " · " + core::versionLabel(version) + "…";
    message_.clear();
    phase_ = Phase::Working;
}

// Takes the result of the job that just ended.
void RestoreScreen::finishJob() {
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
        if (shared_.restored) {
            message_ = "Saved to Checkpoint as " + shared_.restored->localVersion + ". Then restore it in Checkpoint.";
            messageColor_ = kOk;
        } else if (shared_.error) {
            message_ = core::describe(*shared_.error);
            messageColor_ = kError;
        }
    }
    phase_ = Phase::Pick;
}

ScreenId RestoreScreen::update(u32 down, const touchPosition* touch) {
    if (phase_ != Phase::Pick) {
        if (!app_.worker.running()) {
            finishJob();
            if (leaving_) {
                return ScreenId::Menu;
            }
            return ScreenId::Restore;
        }
        if ((down & KEY_B) || (phase_ == Phase::Working && touch != nullptr && Ui::hit(cancel_, *touch))) {
            app_.cancel.requested = true;
            leaving_ = phase_ == Phase::Loading;
        }
        return ScreenId::Restore;
    }

    if (touch != nullptr) {
        if (auto row = versions_.rowAt(*touch)) {
            versions_.select(*row);
            onVersions_ = true;
        } else if (Ui::hit(download_, *touch) && !versions_.empty()) {
            startDownload();
        }
        return ScreenId::Restore;
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
        return ScreenId::Restore;
    }
    if (down & (KEY_B | KEY_DLEFT)) {
        onVersions_ = false;
        return ScreenId::Restore;
    }
    versions_.handle(down);
    if (down & KEY_A) {
        startDownload();
    }
    return ScreenId::Restore;
}

void RestoreScreen::draw() {
    Ui& ui = app_.ui;
    if (phase_ == Phase::Loading) {
        Shared s;
        {
            std::lock_guard lock(mutex_);
            s.loaded = shared_.loaded;
            s.total = shared_.total;
        }
        ui.bar(GFX_TOP, "Restore · pick a game", "");
        std::string line = "Loading backups from Koofr…";
        if (s.total > 0) {
            line += " " + std::to_string(s.loaded) + " / " + std::to_string(s.total);
        }
        ui.text(GFX_TOP, 10.0f, 30.0f, line, kCurrent);
        footHint(ui, "B — back");
        return;
    }

    games_.draw(ui, "Restore · pick a game", phase_ == Phase::Pick && !onVersions_);
    if (!message_.empty()) {
        ui.text(GFX_TOP, 10.0f, GameListView::kBottom + 2.0f, message_, messageColor_, 0.45f, kTopWidth - 20.0f);
    }

    if (phase_ == Phase::Working) {
        ui.text(GFX_BOTTOM, 10.0f, 40.0f, working_, kCurrent, 0.45f, kBottomWidth - 20.0f);
        Button b = cancel_;
        if (app_.cancel.cancelled()) {
            b.label = "Cancelling…";
            b.disabled = true;
        }
        ui.button(b);
        return;
    }

    const CloudTitle* t = games_.current();
    ui.bar(GFX_BOTTOM, t != nullptr ? t->name : "", "");
    versions_.draw(ui, onVersions_);
    Button b = download_;
    b.disabled = versions_.empty();
    ui.button(b);
    ui.centered(GFX_BOTTOM, 200.0f, "Then restore it in Checkpoint.", kMuted, 0.45f);
    footHint(ui, onVersions_ ? "A — download   B — games" : "A — versions   B — menu");
}

}  // namespace ui
