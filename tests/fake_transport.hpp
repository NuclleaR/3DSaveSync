#pragma once

#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/http.hpp"
#include "core/url.hpp"

// Records every request and answers with queued replies, in order.
struct FakeTransport : core::HttpTransport {
    struct Reply {
        std::optional<core::Error> error;
        int status = 0;
        std::string body;
    };

    std::vector<core::HttpRequest> requests;
    std::vector<std::string> uploads;  // drained stream bodies, one per request with a streamBody
    std::deque<Reply> replies;

    void queue(int status, std::string body = "") { replies.push_back({std::nullopt, status, std::move(body)}); }
    void queueError(core::Error e) { replies.push_back({std::move(e), 0, ""}); }

    std::string header(size_t i, std::string_view name) const {
        for (const auto& [k, v] : requests.at(i).headers) {
            if (k == name) {
                return v;
            }
        }
        return "<missing>";
    }

    std::expected<core::HttpResponse, core::Error> send(const core::HttpRequest& req, core::BodySink* sink) override {
        requests.push_back(req);
        if (req.streamBody) {
            std::string data;
            char buf[4];
            for (;;) {
                auto n = req.streamBody->read(buf, sizeof(buf));
                if (!n) {
                    return std::unexpected(n.error());
                }
                if (*n == 0) {
                    break;
                }
                data.append(buf, *n);
            }
            uploads.push_back(std::move(data));
        }
        if (replies.empty()) {
            return core::HttpResponse{500, "no reply queued"};
        }
        Reply r = std::move(replies.front());
        replies.pop_front();
        if (r.error) {
            return std::unexpected(*r.error);
        }
        if (sink != nullptr && r.status >= 200 && r.status < 300) {
            auto w = sink->write(r.body.data(), r.body.size());
            if (!w) {
                return std::unexpected(w.error());
            }
            return core::HttpResponse{r.status, ""};
        }
        return core::HttpResponse{r.status, r.body};
    }
};

// A Koofr-style multistatus for folder `selfHref` (e.g. "/dav/Koofr/r/a b")
// containing plain files with the given names.
inline std::string multistatusWith(const std::string& selfHref, const std::vector<std::string>& files) {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?><D:multistatus xmlns:D="DAV:">)";
    xml += "<D:response><D:href>" + core::encodePath(selfHref) +
           "</D:href><D:propstat><D:prop><D:resourcetype><D:collection/></D:resourcetype></D:prop></D:propstat></D:response>";
    for (const auto& f : files) {
        xml += "<D:response><D:href>" + core::encodePath(selfHref + "/" + f) +
               "</D:href><D:propstat><D:prop><D:resourcetype/><D:getcontentlength>1</D:getcontentlength></D:prop></D:propstat></D:response>";
    }
    xml += "</D:multistatus>";
    return xml;
}
