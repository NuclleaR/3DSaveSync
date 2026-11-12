#include <curl/curl.h>

#include <string>

#include "doctest.h"
#include "platform/curl_error.hpp"

using core::ErrorKind;
using platform::curlError;

TEST_CASE("certificate and handshake failures map to Tls") {
    CHECK(curlError(CURLE_PEER_FAILED_VERIFICATION).kind == ErrorKind::Tls);
    CHECK(curlError(CURLE_SSL_CACERT_BADFILE).kind == ErrorKind::Tls);
    CHECK(curlError(CURLE_SSL_CONNECT_ERROR).kind == ErrorKind::Tls);
}

TEST_CASE("connection failures map to Network") {
    CHECK(curlError(CURLE_COULDNT_RESOLVE_HOST).kind == ErrorKind::Network);
    CHECK(curlError(CURLE_COULDNT_CONNECT).kind == ErrorKind::Network);
    CHECK(curlError(CURLE_OPERATION_TIMEDOUT).kind == ErrorKind::Network);
    CHECK(curlError(CURLE_SEND_ERROR).kind == ErrorKind::Network);
    CHECK(curlError(CURLE_RECV_ERROR).kind == ErrorKind::Network);
}

TEST_CASE("an aborted transfer maps to Cancelled") {
    CHECK(curlError(CURLE_ABORTED_BY_CALLBACK).kind == ErrorKind::Cancelled);
}

TEST_CASE("other curl errors map to Network with curl's message") {
    const auto e = curlError(CURLE_GOT_NOTHING);
    CHECK(e.kind == ErrorKind::Network);
    CHECK(e.detail == std::string(curl_easy_strerror(CURLE_GOT_NOTHING)));
}

TEST_CASE("a wrong console clock reads as a date problem") {
    CHECK(core::describe(curlError(CURLE_PEER_FAILED_VERIFICATION)) ==
          "Secure connection failed. Check the console's date and time.");
}
