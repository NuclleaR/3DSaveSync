#pragma once

#include "core/checkpoint.hpp"

namespace platform {

// The console's clock. The 3DS keeps local time and no time zone, so time()
// already is local time; localtime() just splits it up.
core::LocalDateTime nowLocal();

}  // namespace platform
