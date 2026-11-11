#pragma once

#include <string>

#include "core/dav.hpp"
#include "core/http.hpp"

namespace core {

inline constexpr const char* PROPFIND_BODY =
    R"(<?xml version="1.0" encoding="utf-8"?><d:propfind xmlns:d="DAV:"><d:prop><d:resourcetype/><d:getcontentlength/></d:prop></d:propfind>)";

// WebDAV over an HttpTransport. baseUrl is e.g. "https://app.koofr.net/dav/Koofr";
// paths passed to the Dav methods are appended to it percent-encoded.
class WebDavClient : public Dav {
public:
    WebDavClient(HttpTransport& transport, std::string baseUrl);

    std::expected<std::vector<DavEntry>, Error> list(const std::string& path) override;
    std::expected<void, Error> makeDirs(const std::string& path) override;
    std::expected<void, Error> put(const std::string& path, BodySource body) override;
    std::expected<std::string, Error> getText(const std::string& path) override;
    std::expected<void, Error> get(const std::string& path, BodySink& sink) override;
    std::expected<void, Error> move(const std::string& from, const std::string& to) override;
    std::expected<void, Error> remove(const std::string& path) override;

private:
    std::string url(const std::string& path) const;
    std::expected<HttpResponse, Error> send(HttpRequest req, BodySink* sink = nullptr);

    HttpTransport& transport_;
    std::string baseUrl_;
    std::string basePath_;  // path component of baseUrl, e.g. "/dav/Koofr"
};

}  // namespace core
