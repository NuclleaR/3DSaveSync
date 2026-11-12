#include "core/cloud_store.hpp"

#include <algorithm>
#include <memory>
#include <set>

namespace core {

namespace {

constexpr std::string_view kManifestSuffix = ".manifest";

std::string parentOf(const std::string& path) {
    const auto slash = path.rfind('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

BodySource textSource(std::string text) {
    auto bytes = std::make_shared<std::string>(std::move(text));
    auto offset = std::make_shared<size_t>(0);
    return BodySource{bytes->size(), [bytes, offset](char* buf, size_t cap) -> std::expected<size_t, Error> {
                          const size_t n = std::min(cap, bytes->size() - *offset);
                          std::copy_n(bytes->data() + *offset, n, buf);
                          *offset += n;
                          return n;
                      }};
}

// Directory listing where a missing folder counts as empty.
std::expected<std::vector<DavEntry>, Error> listOrEmpty(Dav& dav, const std::string& path) {
    auto entries = dav.list(path);
    if (!entries && entries.error().kind == ErrorKind::NotFound) {
        return std::vector<DavEntry>{};
    }
    return entries;
}

}  // namespace

CloudStore::CloudStore(Dav& dav, std::string root) : dav_(dav), root_(std::move(root)) {}

std::string CloudStore::savesDir(const std::string& title, const std::string& version) const {
    return root_ + "/saves/" + title + "/" + version;
}

std::string CloudStore::extdataDir(const std::string& title, const std::string& version) const {
    return root_ + "/extdata/" + title + "/" + version;
}

std::string CloudStore::manifestPath(const std::string& title, const std::string& version) const {
    return root_ + "/manifests/" + title + "/" + version + std::string(kManifestSuffix);
}

std::string CloudStore::incomingDir(const std::string& title, const std::string& version) const {
    return root_ + "/.incoming/" + title + "/" + version;
}

std::expected<std::vector<std::string>, Error> CloudStore::listTitles() {
    auto entries = listOrEmpty(dav_, root_ + "/manifests");
    if (!entries) {
        return std::unexpected(entries.error());
    }
    std::vector<std::string> titles;
    for (const auto& e : *entries) {
        if (e.isDir) {
            titles.push_back(e.name);
        }
    }
    std::sort(titles.begin(), titles.end());
    return titles;
}

std::expected<std::vector<std::string>, Error> CloudStore::listVersions(const std::string& title) {
    auto entries = listOrEmpty(dav_, root_ + "/manifests/" + title);
    if (!entries) {
        return std::unexpected(entries.error());
    }
    std::vector<std::string> versions;
    for (const auto& e : *entries) {
        if (!e.isDir && e.name.size() > kManifestSuffix.size() && e.name.ends_with(kManifestSuffix)) {
            versions.push_back(e.name.substr(0, e.name.size() - kManifestSuffix.size()));
        }
    }
    std::sort(versions.begin(), versions.end(), std::greater<>());
    return versions;
}

std::expected<std::optional<Manifest>, Error> CloudStore::latestManifest(const std::string& title) {
    auto versions = listVersions(title);
    if (!versions) {
        return std::unexpected(versions.error());
    }
    if (versions->empty()) {
        return std::optional<Manifest>{};
    }
    auto m = manifest(title, versions->front());
    if (!m) {
        return std::unexpected(m.error());
    }
    return std::optional<Manifest>{std::move(*m)};
}

std::expected<Manifest, Error> CloudStore::manifest(const std::string& title, const std::string& version) {
    auto text = dav_.getText(manifestPath(title, version));
    if (!text) {
        return std::unexpected(text.error());
    }
    auto m = parseManifest(*text);
    if (!m) {
        return std::unexpected(Error{ErrorKind::Protocol, "Invalid manifest " + title + "/" + version});
    }
    return std::move(*m);
}

std::expected<void, Error> CloudStore::uploadVersion(const std::string& title, const std::string& version,
                                                     const Manifest& m, FileSource& files, const CancelToken& cancel) {
    const std::string incoming = incomingDir(title, version);
    std::set<std::string> madeDirs;
    auto ensureDir = [&](const std::string& dir) -> std::expected<void, Error> {
        if (madeDirs.count(dir) != 0) {
            return {};
        }
        auto r = dav_.makeDirs(dir);
        if (r) {
            madeDirs.insert(dir);
        }
        return r;
    };

    for (const FileEntry& f : m.files) {
        if (cancel.cancelled()) {
            return std::unexpected(Error{ErrorKind::Cancelled, "upload cancelled"});
        }
        const std::string target = incoming + (f.part == Part::Save ? "/saves/" : "/extdata/") + f.path;
        if (auto r = ensureDir(parentOf(target)); !r) {
            return r;
        }
        auto body = files.open(f);
        if (!body) {
            return std::unexpected(body.error());
        }
        if (auto r = dav_.put(target, std::move(*body)); !r) {
            return r;
        }
    }
    if (cancel.cancelled()) {
        return std::unexpected(Error{ErrorKind::Cancelled, "upload cancelled"});
    }

    const std::string incomingManifest = incoming + "/" + version + std::string(kManifestSuffix);
    if (auto r = ensureDir(incoming); !r) {
        return r;
    }
    if (auto r = dav_.put(incomingManifest, textSource(serializeManifest(m))); !r) {
        return r;
    }
    if (hasPart(m, Part::Save)) {
        if (auto r = dav_.makeDirs(root_ + "/saves/" + title); !r) {
            return r;
        }
        if (auto r = dav_.move(incoming + "/saves", savesDir(title, version)); !r) {
            return r;
        }
    }
    if (hasPart(m, Part::Extdata)) {
        if (auto r = dav_.makeDirs(root_ + "/extdata/" + title); !r) {
            return r;
        }
        if (auto r = dav_.move(incoming + "/extdata", extdataDir(title, version)); !r) {
            return r;
        }
    }
    if (auto r = dav_.makeDirs(root_ + "/manifests/" + title); !r) {
        return r;
    }
    if (auto r = dav_.move(incomingManifest, manifestPath(title, version)); !r) {
        return r;
    }
    return dav_.remove(incoming);
}

std::expected<void, Error> CloudStore::download(const std::string& title, const std::string& version,
                                                const FileEntry& f, BodySink& sink) {
    const std::string dir = f.part == Part::Save ? savesDir(title, version) : extdataDir(title, version);
    return dav_.get(dir + "/" + f.path, sink);
}

std::expected<void, Error> CloudStore::deleteVersion(const std::string& title, const std::string& version) {
    if (auto r = dav_.remove(savesDir(title, version)); !r) {
        return r;
    }
    if (auto r = dav_.remove(extdataDir(title, version)); !r) {
        return r;
    }
    return dav_.remove(manifestPath(title, version));
}

std::expected<bool, Error> CloudStore::hasManifest(const std::string& title, const std::string& version) {
    auto versions = listVersions(title);
    if (!versions) {
        return std::unexpected(versions.error());
    }
    return std::find(versions->begin(), versions->end(), version) != versions->end();
}

std::expected<void, Error> CloudStore::cleanupIncomplete() {
    const std::string incomingRoot = root_ + "/.incoming";
    auto titles = listOrEmpty(dav_, incomingRoot);
    if (!titles) {
        return std::unexpected(titles.error());
    }

    // Decide everything first, so that no listing failure can leave a
    // half-applied cleanup behind.
    struct Leftover {
        std::string title;
        std::string version;
        bool complete;
    };
    std::vector<Leftover> leftovers;
    for (const auto& t : *titles) {
        if (!t.isDir) {
            continue;
        }
        auto versions = listOrEmpty(dav_, incomingRoot + "/" + t.name);
        if (!versions) {
            return std::unexpected(versions.error());
        }
        for (const auto& v : *versions) {
            if (!v.isDir) {
                continue;
            }
            auto complete = hasManifest(t.name, v.name);
            if (!complete) {
                return std::unexpected(complete.error());
            }
            leftovers.push_back({t.name, v.name, *complete});
        }
    }

    for (const auto& l : leftovers) {
        if (!l.complete) {
            if (auto r = dav_.remove(savesDir(l.title, l.version)); !r) {
                return r;
            }
            if (auto r = dav_.remove(extdataDir(l.title, l.version)); !r) {
                return r;
            }
        }
        if (auto r = dav_.remove(incomingDir(l.title, l.version)); !r) {
            return r;
        }
    }
    return {};
}

}  // namespace core
