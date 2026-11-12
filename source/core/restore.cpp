#include "core/restore.hpp"

#include <tuple>

#include "core/checkpoint.hpp"

namespace core {

namespace {

// Writes downloaded bytes to the SD file and hashes them on the way.
class HashingSink : public BodySink {
public:
    HashingSink(LocalWriter& writer, Hasher& hasher) : writer_(writer), hasher_(hasher) {}
    std::expected<void, Error> write(const char* data, size_t len) override {
        hasher_.update(data, len);
        return writer_.write(data, len);
    }

private:
    LocalWriter& writer_;
    Hasher& hasher_;
};

std::string parentOf(const std::string& path) {
    return path.substr(0, path.rfind('/'));
}

std::expected<void, Error> restoreFile(CloudStore& cloud, LocalFs& fs, const HasherFactory& hasher,
                                       const std::string& title, const std::string& version, const FileEntry& f,
                                       const std::string& target) {
    if (auto r = fs.makeDirs(parentOf(target)); !r) {
        return r;
    }
    auto writer = fs.create(target);
    if (!writer) {
        return std::unexpected(writer.error());
    }
    auto h = hasher();
    HashingSink sink(**writer, *h);
    if (auto r = cloud.download(title, version, f, sink); !r) {
        std::ignore = (*writer)->close();  // the download error is the one worth reporting
        return r;
    }
    if (auto r = (*writer)->close(); !r) {
        return r;
    }
    if (h->hexDigest() != f.sha256) {
        return std::unexpected(Error{ErrorKind::Protocol, "Downloaded file is corrupted"});
    }
    return {};
}

}  // namespace

std::expected<RestoreResult, Error> restoreVersion(CloudStore& cloud, LocalFs& fs, const HasherFactory& hasher,
                                                   const std::string& title, const std::string& version,
                                                   const CancelToken& cancel) {
    auto m = cloud.manifest(title, version);
    if (!m) {
        return std::unexpected(m.error());
    }

    const std::string savesTitle = std::string(kCheckpointSavesRoot) + "/" + title;
    const std::string extdataTitle = std::string(kCheckpointExtdataRoot) + "/" + title;
    std::vector<std::string> taken = fs.listDirs(savesTitle);
    for (auto& d : fs.listDirs(extdataTitle)) {
        taken.push_back(std::move(d));
    }
    const std::string local = uniqueName(version, taken);
    const std::string saveDir = savesTitle + "/" + local;
    const std::string extdataDir = extdataTitle + "/" + local;

    // Both folder names are fresh, so removing them on failure only removes
    // what this call wrote.
    auto fail = [&](Error e) -> std::expected<RestoreResult, Error> {
        // Best effort: the original error is the one worth reporting.
        std::ignore = fs.removeTree(saveDir);
        std::ignore = fs.removeTree(extdataDir);
        return std::unexpected(std::move(e));
    };

    for (const FileEntry& f : m->files) {
        if (cancel.cancelled()) {
            return fail(Error{ErrorKind::Cancelled, "restore cancelled"});
        }
        const std::string target = (f.part == Part::Save ? saveDir : extdataDir) + "/" + f.path;
        if (auto r = restoreFile(cloud, fs, hasher, title, version, f, target); !r) {
            return fail(r.error());
        }
    }
    return RestoreResult{local};
}

}  // namespace core
