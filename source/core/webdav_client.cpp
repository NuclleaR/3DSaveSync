#include "core/webdav_client.hpp"

#include <optional>

#include "core/propfind.hpp"
#include "core/url.hpp"

namespace core {

namespace {

constexpr int kMkcolExists = 405;  // Koofr's answer to MKCOL on an existing folder

bool isSuccess(int status) {
    return status >= 200 && status < 300;
}

std::optional<Error> errorForStatus(int status) {
    if (isSuccess(status)) {
        return std::nullopt;
    }
    const std::string detail = "HTTP " + std::to_string(status);
    switch (status) {
        case 401:
        case 403: return Error{ErrorKind::Auth, detail};
        case 404: return Error{ErrorKind::NotFound, detail};
        case 409:
        case 412: return Error{ErrorKind::Conflict, detail};
        case 507: return Error{ErrorKind::Quota, detail};
        default: return Error{status >= 500 ? ErrorKind::Server : ErrorKind::Protocol, detail};
    }
}

std::string pathOfUrl(const std::string& url) {
    const auto scheme = url.find("://");
    const auto start = url.find('/', scheme == std::string::npos ? 0 : scheme + 3);
    return start == std::string::npos ? std::string() : url.substr(start);
}

}  // namespace

WebDavClient::WebDavClient(HttpTransport& transport, std::string baseUrl)
    : transport_(transport), baseUrl_(std::move(baseUrl)), basePath_(pathOfUrl(baseUrl_)) {}

std::string WebDavClient::url(const std::string& path) const {
    return baseUrl_ + encodePath(path);
}

std::expected<HttpResponse, Error> WebDavClient::send(HttpRequest req, BodySink* sink) {
    return transport_.send(req, sink);
}

std::expected<std::vector<DavEntry>, Error> WebDavClient::list(const std::string& path) {
    auto res = send({"PROPFIND", url(path), {{"Depth", "1"}, {"Content-Type", "application/xml"}}, PROPFIND_BODY, std::nullopt});
    if (!res) {
        return std::unexpected(res.error());
    }
    if (auto err = errorForStatus(res->status)) {
        return std::unexpected(*err);
    }
    return parsePropfind(res->body, basePath_, path);
}

std::expected<void, Error> WebDavClient::makeDirs(const std::string& path) {
    std::string prefix;
    size_t pos = 0;
    while (pos < path.size()) {
        const auto next = path.find('/', pos + 1);
        prefix = path.substr(0, next);
        pos = next == std::string::npos ? path.size() : next;
        if (prefix.empty() || prefix == "/") {
            continue;
        }
        auto res = send({"MKCOL", url(prefix), {}, "", std::nullopt});
        if (!res) {
            return std::unexpected(res.error());
        }
        if (res->status == kMkcolExists) {
            continue;
        }
        if (auto err = errorForStatus(res->status)) {
            return std::unexpected(*err);
        }
    }
    return {};
}

std::expected<void, Error> WebDavClient::put(const std::string& path, BodySource body) {
    auto res = send({"PUT", url(path), {{"Content-Type", "application/octet-stream"}}, "", std::move(body)});
    if (!res) {
        return std::unexpected(res.error());
    }
    if (auto err = errorForStatus(res->status)) {
        return std::unexpected(*err);
    }
    return {};
}

std::expected<std::string, Error> WebDavClient::getText(const std::string& path) {
    auto res = send({"GET", url(path), {}, "", std::nullopt});
    if (!res) {
        return std::unexpected(res.error());
    }
    if (auto err = errorForStatus(res->status)) {
        return std::unexpected(*err);
    }
    return std::move(res->body);
}

std::expected<void, Error> WebDavClient::get(const std::string& path, BodySink& sink) {
    auto res = send({"GET", url(path), {}, "", std::nullopt}, &sink);
    if (!res) {
        return std::unexpected(res.error());
    }
    if (auto err = errorForStatus(res->status)) {
        return std::unexpected(*err);
    }
    return {};
}

std::expected<void, Error> WebDavClient::move(const std::string& from, const std::string& to) {
    auto res = send({"MOVE", url(from), {{"Destination", url(to)}, {"Overwrite", "F"}}, "", std::nullopt});
    if (!res) {
        return std::unexpected(res.error());
    }
    if (auto err = errorForStatus(res->status)) {
        return std::unexpected(*err);
    }
    return {};
}

std::expected<void, Error> WebDavClient::remove(const std::string& path) {
    auto res = send({"DELETE", url(path), {}, "", std::nullopt});
    if (!res) {
        return std::unexpected(res.error());
    }
    if (res->status == 404) {
        return {};
    }
    if (auto err = errorForStatus(res->status)) {
        return std::unexpected(*err);
    }
    return {};
}

}  // namespace core
