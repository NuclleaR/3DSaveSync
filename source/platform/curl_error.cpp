#include "platform/curl_error.hpp"

namespace platform {

core::Error curlError(CURLcode code) {
    core::ErrorKind kind = core::ErrorKind::Network;
    switch (code) {
        case CURLE_PEER_FAILED_VERIFICATION:
        case CURLE_SSL_CACERT_BADFILE:
        case CURLE_SSL_CONNECT_ERROR:
            kind = core::ErrorKind::Tls;
            break;
        case CURLE_ABORTED_BY_CALLBACK:
            kind = core::ErrorKind::Cancelled;
            break;
        default:
            // CURLE_COULDNT_RESOLVE_HOST, CURLE_COULDNT_CONNECT,
            // CURLE_OPERATION_TIMEDOUT, CURLE_SEND_ERROR, CURLE_RECV_ERROR and
            // everything else.
            break;
    }
    return core::Error{kind, curl_easy_strerror(code)};
}

}  // namespace platform
