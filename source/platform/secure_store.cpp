#include "platform/secure_store.hpp"

#include <3ds.h>

#include <cstdio>
#include <string>

namespace platform {

namespace {

constexpr const char* kSettingsPath = "/settings.txt";

core::Error ioError(const char* what, Result rc) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s failed (0x%08lX)", what, static_cast<unsigned long>(rc));
    return core::Error{core::ErrorKind::Io, buf};
}

// The app's own save archive. A fresh install has none yet: format it once.
std::expected<FS_Archive, core::Error> openOwnSave() {
    FS_Archive archive = 0;
    if (R_SUCCEEDED(FSUSER_OpenArchive(&archive, ARCHIVE_SAVEDATA, fsMakePath(PATH_EMPTY, "")))) {
        return archive;
    }
    Result rc = FSUSER_FormatSaveData(ARCHIVE_SAVEDATA, fsMakePath(PATH_EMPTY, ""), 0x200, 10, 10, 11, 11, true);
    if (R_FAILED(rc)) {
        return std::unexpected(ioError("Formatting save data", rc));
    }
    rc = FSUSER_OpenArchive(&archive, ARCHIVE_SAVEDATA, fsMakePath(PATH_EMPTY, ""));
    if (R_FAILED(rc)) {
        return std::unexpected(ioError("Opening save data", rc));
    }
    return archive;
}

}  // namespace

std::expected<std::optional<core::Settings>, core::Error> loadSettings() {
    auto archive = openOwnSave();
    if (!archive) {
        return std::unexpected(archive.error());
    }
    Handle file = 0;
    if (R_FAILED(FSUSER_OpenFile(&file, *archive, fsMakePath(PATH_ASCII, kSettingsPath), FS_OPEN_READ, 0))) {
        FSUSER_CloseArchive(*archive);
        return std::optional<core::Settings>{};
    }
    u64 size = 0;
    Result rc = FSFILE_GetSize(file, &size);
    std::string text;
    u32 got = 0;
    if (R_SUCCEEDED(rc)) {
        text.resize(static_cast<size_t>(size));
        rc = FSFILE_Read(file, &got, 0, text.data(), static_cast<u32>(text.size()));
    }
    FSFILE_Close(file);
    FSUSER_CloseArchive(*archive);
    if (R_FAILED(rc)) {
        return std::unexpected(ioError("Reading settings", rc));
    }
    text.resize(got);
    return core::parseSettings(text);
}

std::expected<void, core::Error> saveSettings(const core::Settings& s) {
    auto archive = openOwnSave();
    if (!archive) {
        return std::unexpected(archive.error());
    }
    const std::string text = core::serializeSettings(s);
    Handle file = 0;
    Result rc = FSUSER_OpenFile(&file, *archive, fsMakePath(PATH_ASCII, kSettingsPath), FS_OPEN_WRITE | FS_OPEN_CREATE, 0);
    if (R_FAILED(rc)) {
        FSUSER_CloseArchive(*archive);
        return std::unexpected(ioError("Opening settings", rc));
    }
    rc = FSFILE_SetSize(file, text.size());
    u32 written = 0;
    if (R_SUCCEEDED(rc)) {
        rc = FSFILE_Write(file, &written, 0, text.data(), static_cast<u32>(text.size()), FS_WRITE_FLUSH);
    }
    FSFILE_Close(file);
    if (R_SUCCEEDED(rc) && written != text.size()) {
        rc = -1;
    }
    if (R_SUCCEEDED(rc)) {
        rc = FSUSER_ControlArchive(*archive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, nullptr, 0, nullptr, 0);
    }
    FSUSER_CloseArchive(*archive);
    if (R_FAILED(rc)) {
        return std::unexpected(ioError("Saving settings", rc));
    }
    return {};
}

}  // namespace platform
