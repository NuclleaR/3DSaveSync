#include "ui/sync_screen.hpp"

#include <algorithm>
#include <cstdio>
#include <mutex>

#include "core/backup.hpp"
#include "core/checkpoint.hpp"
#include "platform/clock.hpp"
#include "platform/ctr_game_source.hpp"
#include "platform/sha256.hpp"

namespace ui {

namespace {

constexpr int kRunRows = 9;

class Progress : public core::BackupProgress {
public:
    Progress(Mutex& m, SyncScreen::State& s) : mutex_(m), state_(s) {}

    void gameStarted(size_t index, size_t total, const core::GameInfo& g) override {
        std::lock_guard lock(mutex_);
        state_.index = index + 1;
        state_.total = total;
        state_.rows.push_back({g.displayName, RowState::Reading, {}});
    }

    void gameFinished(size_t, const core::GameResult& r) override {
        std::lock_guard lock(mutex_);
        state_.finished += 1;
        if (state_.rows.empty()) {
            return;
        }
        SyncRow& row = state_.rows.back();
        switch (r.outcome) {
            case core::Outcome::Uploaded: row.state = RowState::Uploaded; break;
            case core::Outcome::Failed:
                row.state = RowState::Error;
                row.error = r.error ? core::describe(*r.error) : "";
                break;
            default: row.state = RowState::Unchanged; break;
        }
    }

private:
    Mutex& mutex_;
    SyncScreen::State& state_;
};

bool isOffline(const core::Error& e) {
    return e.kind == core::ErrorKind::Network || e.kind == core::ErrorKind::Tls || e.kind == core::ErrorKind::Auth;
}

std::string stamp(const core::LocalDateTime& t) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", t.year, t.month, t.day, t.hour, t.minute);
    return buf;
}

u32 stateColor(RowState s) {
    switch (s) {
        case RowState::Uploaded: return kOk;
        case RowState::Unchanged: return kMuted;
        case RowState::Error: return kError;
        default: return kCurrent;
    }
}

const char* stateText(RowState s) {
    switch (s) {
        case RowState::Uploaded: return "uploaded";
        case RowState::Unchanged: return "unchanged";
        case RowState::Error: return "error";
        default: return "reading…";
    }
}

// A game row: colored marker, name, state on the right.
void drawRow(Ui& ui, float y, const SyncRow& r) {
    const u32 color = stateColor(r.state);
    ui.rect(GFX_TOP, 10.0f, y + 5.0f, 6.0f, 6.0f, color);
    const char* state = stateText(r.state);
    const float stateWidth = ui.textWidth(state);
    ui.text(GFX_TOP, 22.0f, y, ui.fit(r.name, kTopWidth - 44.0f - stateWidth), kText);
    ui.text(GFX_TOP, kTopWidth - 10.0f - stateWidth, y, state, color);
}

}  // namespace

SyncScreen::SyncScreen(App& app) : app_(app), cancel_{60.0f, 130.0f, 200.0f, 36.0f, "Cancel"} {}

SyncScreen::State SyncScreen::copy() {
    std::lock_guard lock(mutex_);
    return state_;
}

void SyncScreen::start() {
    ran_ = true;
    {
        std::lock_guard lock(mutex_);
        state_ = State{};
        state_.phase = "Cleaning up unfinished uploads…";
    }
    app_.cancel.requested = false;
    const core::Settings s = app_.settings;
    if (!app_.worker.start([this, s] { job(s); })) {
        std::lock_guard lock(mutex_);
        state_.done = true;
        state_.heading = "Sync did not start";
        state_.fatal = "Could not start the background task.";
    }
}

void SyncScreen::job(core::Settings s) {
    Koofr koofr(s, &app_.cancel);
    std::string cleanup;
    if (auto cleaned = koofr.cloud.cleanupIncomplete(); !cleaned) {
        if (core::isFatal(cleaned.error())) {
            std::lock_guard lock(mutex_);
            state_.done = true;
            state_.heading = "Sync stopped · " + stamp(platform::nowLocal());
            state_.fatal = core::describe(cleaned.error());
            state_.offline = isOffline(cleaned.error());
            return;
        }
        cleanup = "Cleanup of unfinished uploads failed: " + core::describe(cleaned.error());
    }
    {
        std::lock_guard lock(mutex_);
        state_.cleanup = cleanup;
        state_.phase = "Looking for installed games…";
    }
    platform::CtrGameSource games(platform::sha256Factory());
    Progress progress(mutex_, state_);
    const auto summary =
        core::runBackup(games, koofr.cloud, core::versionName(platform::nowLocal()), progress, app_.cancel);

    std::lock_guard lock(mutex_);
    state_.done = true;
    state_.heading = std::string(summary.fatal ? "Sync stopped" : "Sync finished") + " · " +
                     stamp(platform::nowLocal());
    state_.counts = core::countsLine(summary);
    state_.uploaded = summary.count(core::Outcome::Uploaded);
    state_.unchanged = summary.count(core::Outcome::Unchanged) + summary.count(core::Outcome::Empty);
    state_.errors = summary.count(core::Outcome::Failed);
    if (summary.fatal) {
        state_.fatal = core::describe(*summary.fatal);
        state_.offline = isOffline(*summary.fatal);
    }
    // The summary lists failed games first, then uploaded ones.
    std::vector<SyncRow> rows;
    for (const auto& r : summary.results) {
        if (r.outcome == core::Outcome::Failed) {
            rows.push_back({r.game.displayName, RowState::Error, r.error ? core::describe(*r.error) : ""});
        }
    }
    for (const auto& r : summary.results) {
        if (r.outcome == core::Outcome::Uploaded) {
            rows.push_back({r.game.displayName, RowState::Uploaded, {}});
        }
    }
    state_.rows = std::move(rows);
}

ScreenId SyncScreen::update(u32 down, const touchPosition* touch) {
    if (!app_.worker.running()) {
        return copy().done ? ScreenId::Menu : ScreenId::Sync;
    }
    if ((down & KEY_B) || (touch != nullptr && Ui::hit(cancel_, *touch))) {
        app_.cancel.requested = true;
    }
    return ScreenId::Sync;
}

void SyncScreen::draw() {
    Ui& ui = app_.ui;
    const State s = copy();
    ui.topBar("Koofr", kMuted, kOk);
    if (s.total == 0) {
        ui.text(GFX_TOP, 10.0f, 30.0f, s.phase, kCurrent);
    } else {
        ui.text(GFX_TOP, 10.0f, 30.0f, "Backing up " + std::to_string(s.index) + " / " + std::to_string(s.total),
                kText);
        const float w = kTopWidth - 20.0f;
        ui.rect(GFX_TOP, 10.0f, 50.0f, w, 8.0f, kLine);
        ui.rect(GFX_TOP, 10.0f, 50.0f, w * static_cast<float>(s.finished) / static_cast<float>(s.total), 8.0f,
                kAccent);
        const size_t first = s.rows.size() > kRunRows ? s.rows.size() - kRunRows : 0;
        for (size_t i = first; i < s.rows.size(); ++i) {
            drawRow(ui, 66.0f + static_cast<float>(i - first) * 16.0f, s.rows[i]);
        }
    }
    ui.centered(GFX_BOTTOM, 60.0f, "Saves are only read.", kMuted, 0.45f);
    ui.centered(GFX_BOTTOM, 76.0f, "Nothing on the console is changed.", kMuted, 0.45f);
    Button b = cancel_;
    if (app_.cancel.cancelled()) {
        b.label = "Cancelling…";
        b.disabled = true;
    }
    ui.button(b);
}

void SyncScreen::drawSummary() {
    Ui& ui = app_.ui;
    if (!ran_) {
        ui.topBar("Koofr", kMuted, kMuted);
        ui.text(GFX_TOP, 10.0f, 30.0f, "Back up on start is off.", kText);
        ui.text(GFX_TOP, 10.0f, 48.0f, "Sync now backs up every game that changed.", kMuted);
        return;
    }
    const State s = copy();
    ui.topBar("Koofr", kMuted, s.offline ? kError : kOk);
    ui.text(GFX_TOP, 10.0f, 30.0f, s.heading, kText);
    float y = 48.0f;
    if (!s.counts.empty()) {
        // "N uploaded · N unchanged · N errors", each part in its color.
        float x = 10.0f;
        auto part = [&](const std::string& t, u32 color) {
            ui.text(GFX_TOP, x, y, t, color);
            x += ui.textWidth(t);
        };
        part(std::to_string(s.uploaded) + " uploaded", kOk);
        part(" · ", kMuted);
        part(std::to_string(s.unchanged) + " unchanged", kMuted);
        part(" · ", kMuted);
        part(std::to_string(s.errors) + " errors", kError);
        y += 20.0f;
    }
    if (!s.fatal.empty()) {
        ui.text(GFX_TOP, 10.0f, y, s.fatal, kError, 0.5f, kTopWidth - 20.0f);
        y += 34.0f;
    }
    if (!s.cleanup.empty()) {
        ui.text(GFX_TOP, 10.0f, y, s.cleanup, kError, 0.45f, kTopWidth - 20.0f);
        y += 30.0f;
    }
    size_t shown = 0;
    for (const auto& r : s.rows) {
        const float need = r.state == RowState::Error && !r.error.empty() ? 30.0f : 16.0f;
        if (y + need > 236.0f) {
            break;
        }
        drawRow(ui, y, r);
        y += 16.0f;
        if (r.state == RowState::Error && !r.error.empty()) {
            ui.text(GFX_TOP, 22.0f, y, ui.fit(r.error, kTopWidth - 32.0f, 0.45f), kError, 0.45f);
            y += 14.0f;
        }
        ++shown;
    }
    if (shown < s.rows.size() && y <= 224.0f) {
        ui.text(GFX_TOP, 22.0f, y, "+" + std::to_string(s.rows.size() - shown) + " more", kMuted, 0.45f);
    }
}

}  // namespace ui
