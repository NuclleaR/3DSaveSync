#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "core/cancel.hpp"
#include "core/cloud_store.hpp"
#include "core/error.hpp"
#include "core/hasher.hpp"

namespace core {

class LocalWriter {
public:
    virtual ~LocalWriter() = default;
    virtual std::expected<void, Error> write(const char* data, size_t len) = 0;
    virtual std::expected<void, Error> close() = 0;
};

// The console's SD card, as far as Restore needs it.
class LocalFs {
public:
    virtual ~LocalFs() = default;
    // Folder names in path; a missing folder → {}; any other failure → Io error.
    virtual std::expected<std::vector<std::string>, Error> listDirs(const std::string& path) = 0;
    virtual std::expected<void, Error> makeDirs(const std::string& path) = 0;
    virtual std::expected<std::unique_ptr<LocalWriter>, Error> create(const std::string& path) = 0;
    virtual std::expected<void, Error> removeTree(const std::string& path) = 0;
};

struct RestoreResult {
    std::string localVersion;  // the folder name used under Checkpoint's folders
};

// Downloads a version into Checkpoint's saves/extdata folders under a folder
// name no existing local backup uses, verifying every file against the
// manifest. On any failure the folders created by this call are removed.
std::expected<RestoreResult, Error> restoreVersion(CloudStore& cloud, LocalFs& fs, const HasherFactory& hasher,
                                                   const std::string& title, const std::string& version,
                                                   const CancelToken& cancel);

}  // namespace core
