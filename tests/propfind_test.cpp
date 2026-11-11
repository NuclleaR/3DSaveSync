#include "third_party/doctest.h"

#include "core/propfind.hpp"
#include "test_util.hpp"

TEST_CASE("parsePropfind real Koofr fixture") {
    auto xml = readFile("fixtures/koofr-propfind.xml");
    auto r = core::parsePropfind(xml, "/dav/Koofr", "/3DSaveSync-probe/src/sub dir");
    REQUIRE(r);
    REQUIRE(r->size() == 1);
    CHECK((*r)[0].name == "file é.txt");
    CHECK_FALSE((*r)[0].isDir);
    CHECK((*r)[0].size == 5);
}

TEST_CASE("parsePropfind prefixes, absolute hrefs, folders") {
    const char* xml = R"(<?xml version="1.0"?><D:multistatus xmlns:D="DAV:">
      <D:response><D:href>https://app.koofr.net/dav/Koofr/r/</D:href><D:propstat><D:prop><D:resourcetype><D:collection/></D:resourcetype></D:prop></D:propstat></D:response>
      <D:response><D:href>/dav/Koofr/r/0x0175E%20Pok%C3%A9mon%20Moon/</D:href><D:propstat><D:prop><D:resourcetype><D:collection/></D:resourcetype></D:prop></D:propstat></D:response>
      <D:response><D:href>/dav/Koofr/r/a.manifest</D:href><D:propstat><D:prop><D:resourcetype/><D:getcontentlength>42</D:getcontentlength></D:prop></D:propstat></D:response>
    </D:multistatus>)";
    auto r = core::parsePropfind(xml, "/dav/Koofr", "/r");
    REQUIRE(r);
    REQUIRE(r->size() == 2);
    CHECK((*r)[0].name == "0x0175E Pokémon Moon");
    CHECK((*r)[0].isDir);
    CHECK((*r)[1].size == 42);
    CHECK_FALSE(core::parsePropfind("<not xml", "/dav/Koofr", "/r"));
}
