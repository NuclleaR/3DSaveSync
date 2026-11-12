#include "platform/curl_transport.hpp"

#include <3ds.h>
#include <curl/curl.h>
#include <malloc.h>

#include <cstdlib>
#include <optional>

#include "platform/curl_error.hpp"

namespace platform {

namespace {

constexpr size_t kSocBufferSize = 0x100000;
constexpr size_t kSocBufferAlign = 0x1000;
constexpr const char* kCaBundle = "romfs:/cacert.pem";
constexpr const char* kUserAgent = "3DSaveSync/0.1";
// Owner, 2026-11-12: 10 s is plenty on Wi-Fi. Applies to opening the
// connection and to a transfer that stops delivering any bytes.
constexpr long kTimeoutSeconds = 10;

u32* socBuffer = nullptr;

// State shared with the curl callbacks for one request.
struct Exchange {
    CURL* curl;
    const core::HttpRequest& req;
    core::BodySink* sink;
    const core::CancelToken* cancel;
    std::string body;
    std::optional<core::Error> error;  // a failure inside a callback
};

bool isSuccess(long status) {
    return status >= 200 && status < 300;
}

size_t onWrite(char* data, size_t size, size_t count, void* user) {
    auto& ex = *static_cast<Exchange*>(user);
    const size_t len = size * count;
    long status = 0;
    curl_easy_getinfo(ex.curl, CURLINFO_RESPONSE_CODE, &status);
    if (ex.sink != nullptr && isSuccess(status)) {
        if (auto written = ex.sink->write(data, len); !written) {
            ex.error = written.error();
            return 0;  // curl stops with CURLE_WRITE_ERROR
        }
        return len;
    }
    ex.body.append(data, len);
    return len;
}

size_t onRead(char* buf, size_t size, size_t count, void* user) {
    auto& ex = *static_cast<Exchange*>(user);
    auto got = ex.req.streamBody->read(buf, size * count);
    if (!got) {
        ex.error = got.error();
        return CURL_READFUNC_ABORT;
    }
    return *got;
}

int onProgress(void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    const auto& ex = *static_cast<Exchange*>(user);
    return ex.cancel != nullptr && ex.cancel->cancelled() ? 1 : 0;
}

}  // namespace

CurlTransport::CurlTransport(std::string user, std::string password, const core::CancelToken* cancel)
    : curl_(curl_easy_init()), user_(std::move(user)), password_(std::move(password)), cancel_(cancel) {}

CurlTransport::~CurlTransport() {
    if (curl_ != nullptr) {
        curl_easy_cleanup(static_cast<CURL*>(curl_));
    }
}

std::expected<core::HttpResponse, core::Error> CurlTransport::send(const core::HttpRequest& req, core::BodySink* sink) {
    CURL* curl = static_cast<CURL*>(curl_);
    if (curl == nullptr) {
        return std::unexpected(core::Error{core::ErrorKind::Network, "curl_easy_init failed"});
    }
    if (cancel_ != nullptr && cancel_->cancelled()) {
        return std::unexpected(core::Error{core::ErrorKind::Cancelled, ""});
    }

    // Reset clears the previous request's options but keeps the open connection.
    curl_easy_reset(curl);
    Exchange ex{curl, req, sink, cancel_, {}, std::nullopt};

    curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kTimeoutSeconds);
    // Below 1 byte/s for kTimeoutSeconds → CURLE_OPERATION_TIMEDOUT (Network).
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, kTimeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, kUserAgent);
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, static_cast<long>(CURLAUTH_BASIC));
    curl_easy_setopt(curl, CURLOPT_USERNAME, user_.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, password_.c_str());
    curl_easy_setopt(curl, CURLOPT_CAINFO, kCaBundle);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, static_cast<long>(CURL_SSLVERSION_TLSv1_2));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, onWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ex);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, onProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ex);

    curl_slist* headers = nullptr;
    for (const auto& [name, value] : req.headers) {
        headers = curl_slist_append(headers, (name + ": " + value).c_str());
    }
    // No "Expect: 100-continue" round trip before large uploads.
    headers = curl_slist_append(headers, "Expect:");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    if (req.streamBody) {
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, onRead);
        curl_easy_setopt(curl, CURLOPT_READDATA, &ex);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(req.streamBody->size));
    } else if (!req.textBody.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.textBody.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(req.textBody.size()));
    }
    if (req.method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req.method.c_str());
    }

    const CURLcode rc = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    if (rc != CURLE_OK) {
        if (ex.error) {
            return std::unexpected(*ex.error);
        }
        return std::unexpected(curlError(rc));
    }
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    return core::HttpResponse{static_cast<int>(status), std::move(ex.body)};
}

bool networkInit() {
    socBuffer = static_cast<u32*>(memalign(kSocBufferAlign, kSocBufferSize));
    if (socBuffer == nullptr) {
        return false;
    }
    if (R_FAILED(socInit(socBuffer, kSocBufferSize))) {
        std::free(socBuffer);
        socBuffer = nullptr;
        return false;
    }
    if (R_FAILED(sslcInit(0))) {
        socExit();
        std::free(socBuffer);
        socBuffer = nullptr;
        return false;
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        sslcExit();
        socExit();
        std::free(socBuffer);
        socBuffer = nullptr;
        return false;
    }
    return true;
}

void networkExit() {
    if (socBuffer == nullptr) {
        return;
    }
    curl_global_cleanup();
    sslcExit();
    socExit();
    std::free(socBuffer);
    socBuffer = nullptr;
}

}  // namespace platform
