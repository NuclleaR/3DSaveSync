#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "core/hasher.hpp"
#include "core/restore.hpp"
#include "test_util.hpp"

// In-memory SD card. Files become visible when their writer is closed.
class FakeFs : public core::LocalFs {
public:
    std::map<std::string, std::string> files;
    std::set<std::string> dirs;
    std::set<std::string> unlistable;  // listDirs fails with Io for these

    void mkfile(const std::string& path, const std::string& content) {
        mkdirs(path.substr(0, path.rfind('/')));
        files[path] = content;
    }
    bool exists(const std::string& path) const {
        if (dirs.count(path) != 0 || files.count(path) != 0) return true;
        for (const auto& [p, _] : files) {
            if (p.starts_with(path + "/")) return true;
        }
        return false;
    }
    std::string file(const std::string& path) const {
        auto it = files.find(path);
        return it == files.end() ? "<missing>" : it->second;
    }

    std::expected<std::vector<std::string>, core::Error> listDirs(const std::string& path) override {
        if (unlistable.count(path) != 0) {
            return std::unexpected(core::Error{core::ErrorKind::Io, "cannot list " + path});
        }
        std::vector<std::string> out;
        const std::string prefix = path + "/";
        for (const auto& d : dirs) {
            if (d.starts_with(prefix) && d.find('/', prefix.size()) == std::string::npos) {
                out.push_back(d.substr(prefix.size()));
            }
        }
        return out;
    }
    std::expected<void, core::Error> makeDirs(const std::string& path) override {
        mkdirs(path);
        return {};
    }
    std::expected<std::unique_ptr<core::LocalWriter>, core::Error> create(const std::string& path) override {
        return std::unique_ptr<core::LocalWriter>(new Writer(*this, path));
    }
    std::expected<void, core::Error> removeTree(const std::string& path) override {
        std::erase_if(files, [&](const auto& kv) { return kv.first == path || kv.first.starts_with(path + "/"); });
        std::erase_if(dirs, [&](const std::string& d) { return d == path || d.starts_with(path + "/"); });
        return {};
    }

private:
    struct Writer : core::LocalWriter {
        FakeFs& fs;
        std::string path;
        std::string data;
        Writer(FakeFs& f, std::string p) : fs(f), path(std::move(p)) {}
        std::expected<void, core::Error> write(const char* d, size_t n) override {
            data.append(d, n);
            return {};
        }
        std::expected<void, core::Error> close() override {
            fs.files[path] = data;
            return {};
        }
    };

    void mkdirs(const std::string& path) {
        for (size_t pos = path.find('/', 1); pos != std::string::npos; pos = path.find('/', pos + 1)) {
            dirs.insert(path.substr(0, pos));
        }
        dirs.insert(path);
    }
};

// Hasher whose digest is fakeDigest() of everything fed to it.
struct FakeHasher : core::Hasher {
    std::string data;
    void update(const char* d, size_t n) override { data.append(d, n); }
    std::string hexDigest() override { return fakeDigest(data); }
};

inline core::HasherFactory fakeHasherFactory() {
    return [] { return std::unique_ptr<core::Hasher>(new FakeHasher); };
}
