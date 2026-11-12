#include "ui/worker.hpp"

#include <utility>

namespace ui {

namespace {

// TLS handshakes and curl run on this thread, so it gets the same stack as
// the main thread (RSF StackSize).
constexpr size_t kStackSize = 0x40000;

}  // namespace

Worker::~Worker() {
    join();
}

bool Worker::start(std::function<void()> job) {
    if (running_) {
        return false;
    }
    join();  // frees the finished thread, if any
    job_ = std::move(job);
    s32 priority = 0x30;
    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    // One step below the UI thread, which mostly waits for VBlank.
    priority = priority < 0x3F ? priority + 1 : priority;
    running_ = true;
    thread_ = threadCreate(entry, this, kStackSize, priority, -2, false);
    if (thread_ == nullptr) {
        running_ = false;
        return false;
    }
    return true;
}

bool Worker::running() const {
    return running_;
}

void Worker::join() {
    if (thread_ == nullptr) {
        return;
    }
    threadJoin(thread_, U64_MAX);
    threadFree(thread_);
    thread_ = nullptr;
}

void Worker::entry(void* self) {
    auto* w = static_cast<Worker*>(self);
    w->job_();
    w->running_ = false;
}

}  // namespace ui
