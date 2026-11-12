// LocalFs over the C library (newlib's sdmc: device on the console). Plain
// POSIX calls only, so it is also built and tested on the PC with a temporary
// folder as the prefix.
#pragma once

#include <string>

#include "core/restore.hpp"

namespace platform {

class SdFs : public core::LocalFs {
public:
    // prefix is put in front of every path, e.g. "sdmc:" + "/3ds/Checkpoint/...".
    explicit SdFs(std::string prefix = "sdmc:");

    std::expected<std::vector<std::string>, core::Error> listDirs(const std::string& path) override;
    std::expected<void, core::Error> makeDirs(const std::string& path) override;
    std::expected<std::unique_ptr<core::LocalWriter>, core::Error> create(const std::string& path) override;
    std::expected<void, core::Error> removeTree(const std::string& path) override;

private:
    std::string prefix_;
};

}  // namespace platform
