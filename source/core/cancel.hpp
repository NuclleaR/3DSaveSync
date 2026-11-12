#pragma once

#include <atomic>

namespace core {

struct CancelToken {
    std::atomic<bool> requested{false};
    bool cancelled() const { return requested.load(); }
};

}  // namespace core
