// curl result codes → core errors. Needs only curl's header, so it is also
// built and tested on the PC.
#pragma once

#include <curl/curl.h>

#include "core/error.hpp"

namespace platform {

// Certificate and handshake failures → Tls; connection failures → Network;
// an aborted transfer → Cancelled; anything else → Network. detail carries
// curl_easy_strerror(code).
core::Error curlError(CURLcode code);

}  // namespace platform
