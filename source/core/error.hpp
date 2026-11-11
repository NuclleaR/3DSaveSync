#pragma once

#include <string>

namespace core {

enum class ErrorKind { Network, Tls, Auth, NotFound, Conflict, Quota, Server, Protocol, Io, Cancelled };

struct Error {
    ErrorKind kind;
    std::string detail;
};

// The text shown to the owner on the console.
std::string describe(const Error& e);

// Errors after which continuing a run is pointless: Network, Tls, Auth, Quota, Cancelled.
bool isFatal(const Error& e);

}  // namespace core
