// The list of files in one backup version (a full snapshot of a game's save
// data and extdata), stored next to the version in the cloud.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace core {

enum class Part { Save, Extdata };

struct FileEntry {
    Part part;
    std::string path;    // relative, '/'-separated, no leading '/'
    uint64_t size;
    std::string sha256;  // 64 lowercase hex digits
};

// A folder in the save data or extdata, so that empty folders come back on
// restore (files carry their own parent folders).
struct DirEntry {
    Part part;
    std::string path;  // relative, '/'-separated, no leading '/'
};

struct Manifest {
    uint64_t titleId = 0;
    std::vector<FileEntry> files;
    std::vector<DirEntry> dirs;
};

// Text format:
//   3dsavesync-manifest 1
//   title 0004000000175E00
//   save<TAB>1024<TAB><sha256><TAB>main
//   extdata<TAB>512<TAB><sha256><TAB>user/data 1.bin
//   savedir<TAB>sub/empty
//   extdatadir<TAB>user
// Files, then folders; each sorted Save before Extdata, then by path. Folder
// lines are optional, so manifests written without them still parse.
std::string serializeManifest(const Manifest& m);
std::optional<Manifest> parseManifest(std::string_view text);

// Same set of (part, path, size, sha256) and of folders; order and titleId
// are ignored.
bool sameContent(const Manifest& a, const Manifest& b);

bool hasPart(const Manifest& m, Part p);

}  // namespace core
