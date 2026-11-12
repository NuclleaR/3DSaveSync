// The backup run on launch (and "Sync again"): progress while it runs, then
// the summary that stays on the top screen under the menu.
#pragma once

#include <string>
#include <vector>

#include "ui/app.hpp"

namespace ui {

enum class RowState { Reading, Uploaded, Unchanged, Error };

struct SyncRow {
    std::string name;
    RowState state = RowState::Reading;
    std::string error;  // describe() text for Error rows
};

class SyncScreen {
public:
    explicit SyncScreen(App& app);

    // Cleans up unfinished uploads, then backs up every game, on the worker.
    void start();
    // B or the Cancel button cancels; returns Menu once the run is over.
    ScreenId update(u32 down, const touchPosition* touch);
    void draw();
    // The summary on the top screen (the menu draws it after the run).
    void drawSummary();
    // false until the first run of this session (auto backup off).
    bool ran() const { return ran_; }

    // State shared with the worker; guarded by mutex.
    struct State {
        std::string phase;  // shown before the first game starts
        size_t index = 0;   // 1-based number of the game being worked on
        size_t total = 0;
        size_t finished = 0;
        std::vector<SyncRow> rows;
        bool done = false;
        std::string heading;  // "Sync finished · YYYY-MM-DD HH:MM"
        std::string counts;   // countsLine()
        int uploaded = 0;
        int unchanged = 0;
        int errors = 0;
        std::string fatal;    // why the run stopped, if it did
        std::string cleanup;  // a cleanup error that did not stop the run
        bool offline = false; // the run stopped on a connection or login error
    };

private:
    State copy();
    void job(core::Settings s);

    App& app_;
    Mutex mutex_;
    State state_;
    Button cancel_;
    bool ran_ = false;
};

}  // namespace ui
