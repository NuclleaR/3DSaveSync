#include "platform/clock.hpp"

#include <ctime>

namespace platform {

core::LocalDateTime nowLocal() {
    const std::time_t now = std::time(nullptr);
    std::tm parts{};
    localtime_r(&now, &parts);
    return {parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec};
}

}  // namespace platform
