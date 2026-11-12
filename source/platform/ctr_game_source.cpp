#include "platform/ctr_game_source.hpp"

#include <3ds.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/checkpoint.hpp"

namespace platform {

namespace {

constexpr size_t kChunk = 64 * 1024;

// SMDH layout: "SMDH" magic, then 16 application titles of 0x200 bytes from
// offset 0x8; title 1 is English and starts with the short description
// (UTF-16, up to 0x40 units).
constexpr size_t kSmdhTitlesOffset = 0x8;
constexpr size_t kSmdhTitleSize = 0x200;
constexpr size_t kSmdhEnglish = 1;
constexpr size_t kShortDescriptionUnits = 0x40;

core::Error ioError(const std::string& what, Result rc) {
    char code[16];
    std::snprintf(code, sizeof(code), "0x%08lX", static_cast<unsigned long>(rc));
    return core::Error{core::ErrorKind::Io, what + " failed (" + code + ")"};
}

std::string toUtf8(const u16* in, size_t maxUnits) {
    std::vector<u16> terminated(in, in + maxUnits);
    terminated.push_back(0);
    std::string out(maxUnits * 3 + 1, '\0');  // a UTF-16 unit is at most 3 UTF-8 bytes
    const ssize_t n = utf16_to_utf8(reinterpret_cast<uint8_t*>(out.data()), terminated.data(), out.size());
    out.resize(n < 0 ? 0 : static_cast<size_t>(n));
    return out;
}

std::vector<u16> toUtf16(const std::string& utf8) {
    std::vector<u16> out(utf8.size() + 1, 0);  // never more UTF-16 units than UTF-8 bytes
    const ssize_t n = utf8_to_utf16(out.data(), reinterpret_cast<const uint8_t*>(utf8.c_str()), utf8.size());
    out.resize(n < 0 ? 0 : static_cast<size_t>(n));
    out.push_back(0);
    return out;
}

FS_Path utf16Path(const std::vector<u16>& p) {
    return FS_Path{PATH_UTF16, static_cast<u32>(p.size() * sizeof(u16)), p.data()};
}

// The archive that holds one part of a title's data.
struct ArchiveSpec {
    FS_ArchiveID id;
    std::array<u32, 3> path;
};

ArchiveSpec archiveFor(core::Part part, uint64_t titleId) {
    if (part == core::Part::Save) {
        return {ARCHIVE_USER_SAVEDATA,
                {MEDIATYPE_SD, static_cast<u32>(titleId), static_cast<u32>(titleId >> 32)}};
    }
    return {ARCHIVE_EXTDATA, {MEDIATYPE_SD, core::extdataIdFor(titleId), 0}};
}

std::expected<FS_Archive, Result> openArchive(core::Part part, uint64_t titleId) {
    const ArchiveSpec spec = archiveFor(part, titleId);
    FS_Archive archive = 0;
    const Result rc = FSUSER_OpenArchive(&archive, spec.id,
                                         FS_Path{PATH_BINARY, static_cast<u32>(sizeof(spec.path)), spec.path.data()});
    if (R_FAILED(rc)) {
        return std::unexpected(rc);
    }
    return archive;
}

bool archiveOpens(core::Part part, uint64_t titleId) {
    auto archive = openArchive(part, titleId);
    if (!archive) {
        return false;
    }
    FSUSER_CloseArchive(*archive);
    return true;
}

// The English short description from the title's SMDH; nullopt when the title
// has no readable SMDH (DLC, data titles).
std::optional<std::string> shortDescription(uint64_t titleId) {
    const std::array<u32, 4> archivePath{static_cast<u32>(titleId), static_cast<u32>(titleId >> 32), MEDIATYPE_SD, 0};
    static constexpr std::array<u32, 5> kIconPath{0, 0, 2, 0x6E6F6369, 0};  // the content's "icon" file
    Handle file = 0;
    Result rc = FSUSER_OpenFileDirectly(
        &file, ARCHIVE_SAVEDATA_AND_CONTENT,
        FS_Path{PATH_BINARY, static_cast<u32>(sizeof(archivePath)), archivePath.data()},
        FS_Path{PATH_BINARY, static_cast<u32>(sizeof(kIconPath)), kIconPath.data()}, FS_OPEN_READ, 0);
    if (R_FAILED(rc)) {
        return std::nullopt;
    }
    std::vector<u8> head(kSmdhTitlesOffset + (kSmdhEnglish + 1) * kSmdhTitleSize);
    u32 got = 0;
    rc = FSFILE_Read(file, &got, 0, head.data(), static_cast<u32>(head.size()));
    FSFILE_Close(file);
    if (R_FAILED(rc) || got != head.size() || std::memcmp(head.data(), "SMDH", 4) != 0) {
        return std::nullopt;
    }
    std::array<u16, kShortDescriptionUnits> units{};
    std::memcpy(units.data(), head.data() + kSmdhTitlesOffset + kSmdhEnglish * kSmdhTitleSize, sizeof(units));
    return toUtf8(units.data(), units.size());
}

struct FoundFile {
    std::string path;  // with a leading '/'
};

// Every file and folder below dir ("" is the archive root), depth first.
std::expected<void, core::Error> walk(FS_Archive archive, const std::string& dir, std::vector<FoundFile>& out,
                                      std::vector<std::string>& dirsOut) {
    const auto path = toUtf16(dir.empty() ? "/" : dir);
    Handle handle = 0;
    Result rc = FSUSER_OpenDirectory(&handle, archive, utf16Path(path));
    if (R_FAILED(rc)) {
        return std::unexpected(ioError("Opening folder " + (dir.empty() ? "/" : dir), rc));
    }
    std::vector<std::string> subdirs;
    std::vector<FS_DirectoryEntry> entries(32);
    for (;;) {
        u32 count = 0;
        rc = FSDIR_Read(handle, &count, static_cast<u32>(entries.size()), entries.data());
        if (R_FAILED(rc) || count == 0) {
            break;
        }
        for (u32 i = 0; i < count; ++i) {
            const std::string child = dir + "/" + toUtf8(entries[i].name, sizeof(entries[i].name) / sizeof(u16));
            if (entries[i].attributes & FS_ATTRIBUTE_DIRECTORY) {
                subdirs.push_back(child);
            } else {
                out.push_back({child});
            }
        }
    }
    FSDIR_Close(handle);
    if (R_FAILED(rc)) {
        return std::unexpected(ioError("Reading folder " + (dir.empty() ? "/" : dir), rc));
    }
    for (const auto& sub : subdirs) {
        dirsOut.push_back(sub);
        if (auto walked = walk(archive, sub, out, dirsOut); !walked) {
            return walked;
        }
    }
    return {};
}

// Size and SHA-256 of one file, read in 64 KiB chunks.
std::expected<std::pair<uint64_t, std::string>, core::Error> hashFile(FS_Archive archive, const std::string& path,
                                                                      core::Hasher& hasher, std::vector<char>& buf) {
    const auto upath = toUtf16(path);
    Handle file = 0;
    Result rc = FSUSER_OpenFile(&file, archive, utf16Path(upath), FS_OPEN_READ, 0);
    if (R_FAILED(rc)) {
        return std::unexpected(ioError("Opening " + path, rc));
    }
    u64 size = 0;
    rc = FSFILE_GetSize(file, &size);
    u64 offset = 0;
    while (R_SUCCEEDED(rc) && offset < size) {
        u32 got = 0;
        const u32 want = static_cast<u32>(std::min<u64>(buf.size(), size - offset));
        rc = FSFILE_Read(file, &got, offset, buf.data(), want);
        if (R_SUCCEEDED(rc) && got == 0) {
            rc = -1;  // shorter than its reported size
        }
        if (R_SUCCEEDED(rc)) {
            hasher.update(buf.data(), got);
            offset += got;
        }
    }
    FSFILE_Close(file);
    if (R_FAILED(rc)) {
        return std::unexpected(ioError("Reading " + path, rc));
    }
    return std::pair<uint64_t, std::string>{size, hasher.hexDigest()};
}

// An open archive file, closed when the last BodySource copy goes away.
struct OpenFile {
    FS_Archive archive = 0;
    Handle file = 0;
    uint64_t offset = 0;

    OpenFile() = default;
    OpenFile(const OpenFile&) = delete;
    OpenFile& operator=(const OpenFile&) = delete;
    ~OpenFile() { close(); }

    void close() {
        if (file != 0) {
            FSFILE_Close(file);
            file = 0;
        }
        if (archive != 0) {
            FSUSER_CloseArchive(archive);
            archive = 0;
        }
    }
};

}  // namespace

CtrGameSource::CtrGameSource(core::HasherFactory hasher) : hasher_(std::move(hasher)) {}

std::expected<std::vector<core::GameInfo>, core::Error> CtrGameSource::games() {
    Result rc = amInit();
    if (R_FAILED(rc)) {
        return std::unexpected(ioError("Starting the title service", rc));
    }
    u32 count = 0;
    rc = AM_GetTitleCount(MEDIATYPE_SD, &count);
    std::vector<u64> ids(count);
    u32 read = 0;
    if (R_SUCCEEDED(rc) && count > 0) {
        rc = AM_GetTitleList(&read, MEDIATYPE_SD, count, ids.data());
    }
    amExit();
    if (R_FAILED(rc)) {
        return std::unexpected(ioError("Listing installed titles", rc));
    }
    ids.resize(read);

    u64 self = 0;
    rc = APT_GetProgramID(&self);
    if (R_FAILED(rc)) {
        // Without it the app's own save (the login) could end up in the cloud.
        return std::unexpected(ioError("Reading this app's title id", rc));
    }

    std::vector<core::GameInfo> out;
    parts_.clear();
    for (const u64 id : ids) {
        if (!core::isBackupCandidate(id, self)) {
            continue;
        }
        auto name = shortDescription(id);
        if (!name) {
            continue;
        }
        const bool save = archiveOpens(core::Part::Save, id);
        const bool extdata = archiveOpens(core::Part::Extdata, id);
        if (!save && !extdata) {
            continue;
        }
        parts_[id] = {save, extdata};
        out.push_back({id, core::titleFolderName(id, core::sanitizeName(*name)), *name});
    }
    std::sort(out.begin(), out.end(),
              [](const core::GameInfo& a, const core::GameInfo& b) { return a.displayName < b.displayName; });
    return out;
}

std::expected<core::Manifest, core::Error> CtrGameSource::snapshot(const core::GameInfo& g) {
    core::Manifest m;
    m.titleId = g.titleId;
    std::vector<char> buf(kChunk);
    const auto listed = parts_.find(g.titleId);
    for (const core::Part part : {core::Part::Save, core::Part::Extdata}) {
        auto archive = openArchive(part, g.titleId);
        if (!archive) {
            const bool hadIt = listed != parts_.end() &&
                               (part == core::Part::Save ? listed->second.first : listed->second.second);
            if (hadIt) {
                return std::unexpected(
                    ioError(part == core::Part::Save ? "Opening the save data" : "Opening the extdata", archive.error()));
            }
            continue;  // the game has no data of this kind
        }
        std::vector<FoundFile> found;
        std::vector<std::string> folders;
        auto walked = walk(*archive, "", found, folders);
        if (!walked) {
            FSUSER_CloseArchive(*archive);
            return std::unexpected(walked.error());
        }
        for (const auto& d : folders) {
            m.dirs.push_back({part, d.substr(1)});
        }
        for (const auto& f : found) {
            auto hasher = hasher_();
            auto hashed = hashFile(*archive, f.path, *hasher, buf);
            if (!hashed) {
                if (part == core::Part::Extdata) {
                    continue;  // in use by the system; Checkpoint skips these too
                }
                FSUSER_CloseArchive(*archive);
                return std::unexpected(hashed.error());
            }
            m.files.push_back({part, f.path.substr(1), hashed->first, std::move(hashed->second)});
        }
        FSUSER_CloseArchive(*archive);
    }
    return m;
}

core::FileSource& CtrGameSource::files(const core::GameInfo& g) {
    files_.titleId = g.titleId;
    return files_;
}

std::expected<core::BodySource, core::Error> CtrGameSource::Files::open(const core::FileEntry& f) {
    auto state = std::make_shared<OpenFile>();
    auto archive = openArchive(f.part, titleId);
    if (!archive) {
        return std::unexpected(ioError("Opening the archive for " + f.path, archive.error()));
    }
    state->archive = *archive;
    const std::string path = "/" + f.path;
    const auto upath = toUtf16(path);
    const Result rc = FSUSER_OpenFile(&state->file, state->archive, utf16Path(upath), FS_OPEN_READ, 0);
    if (R_FAILED(rc)) {
        state->file = 0;
        return std::unexpected(ioError("Opening " + path, rc));
    }
    const uint64_t size = f.size;
    return core::BodySource{
        size, [state, size, path](char* buf, size_t cap) -> std::expected<size_t, core::Error> {
            if (state->offset >= size) {
                state->close();
                return size_t{0};
            }
            u32 got = 0;
            const u32 want = static_cast<u32>(std::min<uint64_t>(cap, size - state->offset));
            const Result read = FSFILE_Read(state->file, &got, state->offset, buf, want);
            if (R_FAILED(read)) {
                return std::unexpected(ioError("Reading " + path, read));
            }
            if (got == 0) {
                return std::unexpected(core::Error{core::ErrorKind::Io, path + " got shorter while uploading"});
            }
            state->offset += got;
            if (state->offset >= size) {
                state->close();
            }
            return static_cast<size_t>(got);
        }};
}

}  // namespace platform
