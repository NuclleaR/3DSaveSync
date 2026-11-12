// HttpTransport over libcurl + mbedTLS (the console's own httpc/sslc TLS stops
// at TLS 1.1, which Koofr rejects).
#pragma once

#include <expected>
#include <string>

#include "core/cancel.hpp"
#include "core/http.hpp"

namespace platform {

class CurlTransport : public core::HttpTransport {
public:
    // cancel may be null. When it is set, the running transfer stops with
    // ErrorKind::Cancelled.
    CurlTransport(std::string user, std::string password, const core::CancelToken* cancel);
    ~CurlTransport() override;
    CurlTransport(const CurlTransport&) = delete;
    CurlTransport& operator=(const CurlTransport&) = delete;

    std::expected<core::HttpResponse, core::Error> send(const core::HttpRequest& req, core::BodySink* sink) override;

private:
    void* curl_;  // CURL*, one handle reused so the TLS connection stays open
    std::string user_;
    std::string password_;
    const core::CancelToken* cancel_;
};

// socInit (0x100000-byte, 0x1000-aligned buffer), sslcInit (mbedTLS's 3DS
// entropy source reads random bytes from sslc) and curl_global_init.
bool networkInit();
void networkExit();

}  // namespace platform
