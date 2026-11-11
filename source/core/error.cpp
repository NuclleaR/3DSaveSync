#include "core/error.hpp"

namespace core {

std::string describe(const Error& e) {
    switch (e.kind) {
        case ErrorKind::Network: return "No connection to Koofr. Check Wi-Fi.";
        case ErrorKind::Tls: return "Secure connection failed. Check the console's date and time.";
        case ErrorKind::Auth: return "Koofr rejected the login. Check email and app password in Settings.";
        case ErrorKind::Quota: return "Koofr storage is full.";
        case ErrorKind::Cancelled: return "Cancelled.";
        default: return "Koofr error: " + e.detail;
    }
}

bool isFatal(const Error& e) {
    switch (e.kind) {
        case ErrorKind::Network:
        case ErrorKind::Tls:
        case ErrorKind::Auth:
        case ErrorKind::Quota:
        case ErrorKind::Cancelled:
            return true;
        default:
            return false;
    }
}

}  // namespace core
