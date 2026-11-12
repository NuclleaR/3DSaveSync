// Backup versions in the cloud, laid out like Checkpoint's folders:
//   <root>/saves/<title>/<version>/...      save data files
//   <root>/extdata/<title>/<version>/...    extdata files (only if the game has any)
//   <root>/manifests/<title>/<version>.manifest
//   <root>/.incoming/<title>/<version>/...  unfinished uploads
// A version is complete exactly when its .manifest exists.
#pragma once

#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "core/cancel.hpp"
#include "core/dav.hpp"
#include "core/error.hpp"
#include "core/http.hpp"
#include "core/manifest.hpp"

namespace core {

class FileSource {
public:
    virtual ~FileSource() = default;
    virtual std::expected<BodySource, Error> open(const FileEntry& f) = 0;
};

class CloudStore {
public:
    CloudStore(Dav& dav, std::string root);

    std::string savesDir(const std::string& title, const std::string& version) const;
    std::string extdataDir(const std::string& title, const std::string& version) const;
    std::string manifestPath(const std::string& title, const std::string& version) const;
    std::string incomingDir(const std::string& title, const std::string& version) const;

    // Folders under <root>/manifests; none yet → {}.
    std::expected<std::vector<std::string>, Error> listTitles();
    // Complete versions of a title, newest first; none → {}.
    std::expected<std::vector<std::string>, Error> listVersions(const std::string& title);
    std::expected<std::optional<Manifest>, Error> latestManifest(const std::string& title);
    std::expected<Manifest, Error> manifest(const std::string& title, const std::string& version);

    // Uploads to <root>/.incoming, then moves saves, extdata and finally the
    // manifest into place (the completion point) and removes the incoming folder.
    std::expected<void, Error> uploadVersion(const std::string& title, const std::string& version, const Manifest& m,
                                             FileSource& files, const CancelToken& cancel);
    std::expected<void, Error> download(const std::string& title, const std::string& version, const FileEntry& f,
                                        BodySink& sink);
    // Removes saves, extdata, then the manifest last, so a failed delete stays visible.
    std::expected<void, Error> deleteVersion(const std::string& title, const std::string& version);
    // For every <root>/.incoming/<title>/<version>: if that version has no
    // manifest, removes its saves/extdata folders (partial moves of the
    // interrupted upload); then removes the incoming folder. Touches nothing
    // else, and deletes nothing if a listing fails.
    std::expected<void, Error> cleanupIncomplete();

private:
    std::expected<bool, Error> hasManifest(const std::string& title, const std::string& version);

    Dav& dav_;
    std::string root_;
};

}  // namespace core
