// Minimal HTTP abstraction so that WebDAV logic can be tested on the PC; the
// 3DS implementation lives in platform/curl_transport.
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/error.hpp"

namespace core {

// A request body read in chunks: read() fills up to cap bytes and returns how
// many it wrote, 0 at the end.
struct BodySource {
    uint64_t size;
    std::function<std::expected<size_t, Error>(char* buf, size_t cap)> read;
};

class BodySink {
public:
    virtual ~BodySink() = default;
    virtual std::expected<void, Error> write(const char* data, size_t len) = 0;
};

struct HttpRequest {
    std::string method;
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string textBody;
    std::optional<BodySource> streamBody;
};

struct HttpResponse {
    int status = 0;
    std::string body;
};

class HttpTransport {
public:
    virtual ~HttpTransport() = default;
    // With a sink and a 2xx status the body goes to the sink; otherwise into
    // HttpResponse::body. Transport failures: Network / Tls / Cancelled.
    virtual std::expected<HttpResponse, Error> send(const HttpRequest& req, BodySink* sink) = 0;
};

}  // namespace core
