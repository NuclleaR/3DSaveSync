// One background job at a time, so network and file work never blocks drawing.
#pragma once

#include <3ds.h>

#include <atomic>
#include <functional>

namespace ui {

class Worker {
public:
    Worker() = default;
    ~Worker();
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    // false if a job is still running or the thread could not be created.
    bool start(std::function<void()> job);
    bool running() const;
    // Waits for the current job, if any.
    void join();

private:
    static void entry(void* self);

    std::function<void()> job_;
    Thread thread_ = nullptr;
    std::atomic<bool> running_{false};
};

// A LightLock usable with std::lock_guard.
class Mutex {
public:
    Mutex() { LightLock_Init(&lock_); }
    void lock() { LightLock_Lock(&lock_); }
    void unlock() { LightLock_Unlock(&lock_); }

private:
    LightLock lock_;
};

}  // namespace ui
