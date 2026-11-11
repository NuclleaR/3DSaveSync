// Naming rules shared with Checkpoint, so that backups this app downloads
// land in folders Checkpoint recognises.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace core {

inline constexpr const char* kCheckpointSavesRoot = "/3ds/Checkpoint/saves";
inline constexpr const char* kCheckpointExtdataRoot = "/3ds/Checkpoint/extdata";

// Checkpoint's removeForbiddenCharacters: each of .,!\/:?*"<>| becomes a
// space, then trailing spaces are stripped. Works byte-wise on UTF-8 because
// every forbidden character is ASCII.
std::string sanitizeName(std::string_view utf8);

// "0x%05X " of the title id's low 32 bits shifted right by 8, then the name.
std::string titleFolderName(uint64_t titleId, std::string_view sanitizedName);

// Drops a leading "0x" + 5 hex digits + space prefix; returns the input
// unchanged when the prefix is absent.
std::string displayNameFromFolder(std::string_view folder);

struct LocalDateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};

// "%04i%02i%02i-%02i%02i%02i", Checkpoint's default backup name.
std::string versionName(const LocalDateTime& t);

// base if free, else the first free base + "-2", "-3", ...
std::string uniqueName(const std::string& base, const std::vector<std::string>& taken);

// Extdata archive id for a title, including Checkpoint's known exceptions.
uint32_t extdataIdFor(uint64_t titleId);

// Instruction manuals, a garbage entry, updates and DSi data archives.
bool isSystemExcluded(uint64_t titleId);

}  // namespace core
