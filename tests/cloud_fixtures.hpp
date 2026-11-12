#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/cloud_store.hpp"
#include "core/manifest.hpp"
#include "fake_dav.hpp"
#include "test_util.hpp"

struct TestFile {
    core::Part part;
    std::string path;
    std::string content;
};

// A manifest whose sizes and digests (fakeDigest) match the given contents.
inline core::Manifest makeManifest(uint64_t titleId, const std::vector<TestFile>& files) {
    core::Manifest m{titleId, {}, {}};
    for (const auto& f : files) {
        m.files.push_back({f.part, f.path, f.content.size(), fakeDigest(f.content)});
    }
    return m;
}

// FileSource over in-memory contents, keyed by (part, path).
class MemFiles : public core::FileSource {
public:
    MemFiles() = default;
    explicit MemFiles(const std::vector<TestFile>& files) { set(files); }

    void set(const std::vector<TestFile>& files) {
        data_.clear();
        for (const auto& f : files) {
            data_[{f.part, f.path}] = f.content;
        }
    }

    std::expected<core::BodySource, core::Error> open(const core::FileEntry& f) override {
        auto it = data_.find({f.part, f.path});
        if (it == data_.end()) {
            return std::unexpected(core::Error{core::ErrorKind::Io, "no such file " + f.path});
        }
        auto bytes = std::make_shared<std::string>(it->second);
        auto offset = std::make_shared<size_t>(0);
        return core::BodySource{bytes->size(), [bytes, offset](char* buf, size_t cap) -> std::expected<size_t, core::Error> {
                                    const size_t n = std::min(cap, bytes->size() - *offset);
                                    std::copy_n(bytes->data() + *offset, n, buf);
                                    *offset += n;
                                    return n;
                                }};
    }

private:
    std::map<std::pair<core::Part, std::string>, std::string> data_;
};

inline std::vector<TestFile> sampleFiles() {
    return {{core::Part::Save, "main", "save-bytes"}, {core::Part::Extdata, "user/data 1.bin", "ext-bytes"}};
}

inline std::vector<TestFile> twoSaveFiles() {
    return {{core::Part::Save, "first", "one"}, {core::Part::Save, "second", "two"}};
}

// A complete version written directly into the fake tree (save file + manifest).
inline void putCompleteVersion(FakeDav& dav, const std::string& title, const std::string& version,
                               const std::string& root = "/R") {
    const std::vector<TestFile> files{{core::Part::Save, "main", "old"}};
    dav.mkfile(root + "/saves/" + title + "/" + version + "/main", "old");
    dav.mkfile(root + "/manifests/" + title + "/" + version + ".manifest",
               core::serializeManifest(makeManifest(1, files)));
}
