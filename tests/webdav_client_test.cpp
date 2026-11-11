#include "third_party/doctest.h"

#include <algorithm>

#include "core/webdav_client.hpp"
#include "fake_transport.hpp"

namespace {

constexpr int EXISTS_CODE = 405;  // Koofr's answer to MKCOL on an existing folder (plan: Probe results)

core::ErrorKind kindFor(int status) {
    FakeTransport t;
    t.queue(status);
    core::WebDavClient dav(t, "https://h/dav/Koofr");
    auto r = dav.list("/r");
    REQUIRE_FALSE(r);
    return r.error().kind;
}

struct StringSink : core::BodySink {
    std::string data;
    std::expected<void, core::Error> write(const char* d, size_t n) override {
        data.append(d, n);
        return {};
    }
};

}  // namespace

TEST_CASE("list sends PROPFIND depth 1 to encoded URL") {
    FakeTransport t;
    t.queue(207, multistatusWith("/dav/Koofr/r/a b", {"x.manifest"}));
    core::WebDavClient dav(t, "https://app.koofr.net/dav/Koofr");
    auto r = dav.list("/r/a b");
    REQUIRE(r);
    CHECK(r->size() == 1);
    CHECK(t.requests[0].method == "PROPFIND");
    CHECK(t.requests[0].url == "https://app.koofr.net/dav/Koofr/r/a%20b");
    CHECK(t.header(0, "Depth") == "1");
    CHECK(t.requests[0].textBody == core::PROPFIND_BODY);
}

TEST_CASE("makeDirs creates every segment, existing is fine") {
    FakeTransport t;
    t.queue(EXISTS_CODE);
    t.queue(201);
    core::WebDavClient dav(t, "https://h/dav/Koofr");
    CHECK(dav.makeDirs("/a/b c"));
    REQUIRE(t.requests.size() == 2);
    CHECK(t.requests[0].method == "MKCOL");
    CHECK(t.requests[0].url == "https://h/dav/Koofr/a");
    CHECK(t.requests[1].url == "https://h/dav/Koofr/a/b%20c");
}

TEST_CASE("move sends Destination and Overwrite F; 412 is Conflict") {
    FakeTransport t;
    t.queue(412);
    core::WebDavClient dav(t, "https://h/dav/Koofr");
    auto r = dav.move("/a/x", "/b/y z");
    REQUIRE_FALSE(r);
    CHECK(r.error().kind == core::ErrorKind::Conflict);
    CHECK(t.requests[0].method == "MOVE");
    CHECK(t.header(0, "Destination") == "https://h/dav/Koofr/b/y%20z");
    CHECK(t.header(0, "Overwrite") == "F");
}

TEST_CASE("put streams the body; get and getText return it") {
    FakeTransport t;
    t.queue(201);
    t.queue(200, "hello");
    t.queue(200, "manifest text");
    core::WebDavClient dav(t, "https://h/dav/Koofr");
    std::string payload = "save-bytes";
    size_t offset = 0;
    core::BodySource src{payload.size(), [&](char* buf, size_t cap) -> std::expected<size_t, core::Error> {
                             size_t n = std::min(cap, payload.size() - offset);
                             std::copy_n(payload.data() + offset, n, buf);
                             offset += n;
                             return n;
                         }};
    CHECK(dav.put("/f é", src));
    CHECK(t.requests[0].method == "PUT");
    CHECK(t.requests[0].url == "https://h/dav/Koofr/f%20%C3%A9");
    CHECK(t.uploads.at(0) == "save-bytes");
    StringSink sink;
    CHECK(dav.get("/f", sink));
    CHECK(sink.data == "hello");
    CHECK(dav.getText("/m").value() == "manifest text");
}

TEST_CASE("status mapping") {
    CHECK(kindFor(401) == core::ErrorKind::Auth);
    CHECK(kindFor(404) == core::ErrorKind::NotFound);
    CHECK(kindFor(507) == core::ErrorKind::Quota);
    CHECK(kindFor(502) == core::ErrorKind::Server);
    CHECK(kindFor(418) == core::ErrorKind::Protocol);
    FakeTransport t;
    t.queue(404);
    core::WebDavClient dav(t, "https://h/d");
    CHECK(dav.remove("/gone"));
    CHECK(t.requests[0].method == "DELETE");
}

TEST_CASE("transport errors pass through") {
    FakeTransport t;
    t.queueError({core::ErrorKind::Tls, "x509"});
    core::WebDavClient dav(t, "https://h/d");
    CHECK(dav.list("/r").error().kind == core::ErrorKind::Tls);
}

TEST_CASE("describe texts") {
    CHECK(core::describe({core::ErrorKind::Tls, ""}) == "Secure connection failed. Check the console's date and time.");
    CHECK(core::describe({core::ErrorKind::Auth, ""}) == "Koofr rejected the login. Check email and app password in Settings.");
    CHECK(core::describe({core::ErrorKind::Quota, ""}) == "Koofr storage is full.");
    CHECK(core::describe({core::ErrorKind::Network, ""}) == "No connection to Koofr. Check Wi-Fi.");
    CHECK(core::describe({core::ErrorKind::Cancelled, ""}) == "Cancelled.");
    CHECK(core::describe({core::ErrorKind::Server, "HTTP 502"}) == "Koofr error: HTTP 502");
    CHECK(core::isFatal({core::ErrorKind::Network, ""}));
    CHECK(core::isFatal({core::ErrorKind::Cancelled, ""}));
    CHECK_FALSE(core::isFatal({core::ErrorKind::Io, ""}));
}
