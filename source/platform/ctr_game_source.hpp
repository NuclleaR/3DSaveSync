// The games installed on the SD card, their save data and extdata. Archives
// are only read: files are opened with FS_OPEN_READ and nothing is committed.
#pragma once

#include <cstdint>
#include <expected>
#include <map>
#include <utility>
#include <vector>

#include "core/backup.hpp"
#include "core/hasher.hpp"

namespace platform {

class CtrGameSource : public core::GameSource {
public:
    explicit CtrGameSource(core::HasherFactory hasher);

    // SD titles minus Checkpoint's exclusions and this app itself, that have a
    // readable SMDH and whose save data or extdata archive opens; sorted by
    // display name.
    std::expected<std::vector<core::GameInfo>, core::Error> games() override;
    std::expected<core::Manifest, core::Error> snapshot(const core::GameInfo& g) override;
    core::FileSource& files(const core::GameInfo& g) override;

private:
    class Files : public core::FileSource {
    public:
        uint64_t titleId = 0;
        std::expected<core::BodySource, core::Error> open(const core::FileEntry& f) override;
    };

    // Which archives opened when games() listed a title: {save, extdata}.
    // snapshot() treats a listed archive that no longer opens as an error, not
    // as "no data", so a version is never uploaded without its save data.
    std::map<uint64_t, std::pair<bool, bool>> parts_;
    core::HasherFactory hasher_;
    Files files_;
};

}  // namespace platform
