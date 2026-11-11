#include "core/checkpoint.hpp"

#include <algorithm>
#include <cstdio>

namespace core {

namespace {

bool isHexDigit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

}  // namespace

std::string sanitizeName(std::string_view utf8) {
    static constexpr std::string_view kForbidden = ".,!\\/:?*\"<>|";
    std::string out(utf8);
    for (char& c : out) {
        if (kForbidden.find(c) != std::string_view::npos) {
            c = ' ';
        }
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

std::string titleFolderName(uint64_t titleId, std::string_view sanitizedName) {
    char prefix[16];
    std::snprintf(prefix, sizeof(prefix), "0x%05X ", static_cast<unsigned>(static_cast<uint32_t>(titleId) >> 8));
    std::string out(prefix);
    out.append(sanitizedName);
    return out;
}

std::string displayNameFromFolder(std::string_view folder) {
    if (folder.size() >= 8 && folder[0] == '0' && folder[1] == 'x' && folder[7] == ' ' &&
        std::all_of(folder.begin() + 2, folder.begin() + 7, isHexDigit)) {
        return std::string(folder.substr(8));
    }
    return std::string(folder);
}

std::string versionName(const LocalDateTime& t) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04i%02i%02i-%02i%02i%02i", t.year, t.month, t.day, t.hour, t.minute, t.second);
    return buf;
}

std::string uniqueName(const std::string& base, const std::vector<std::string>& taken) {
    auto isTaken = [&](const std::string& name) {
        return std::find(taken.begin(), taken.end(), name) != taken.end();
    };
    if (!isTaken(base)) {
        return base;
    }
    for (int n = 2;; ++n) {
        std::string candidate = base + "-" + std::to_string(n);
        if (!isTaken(candidate)) {
            return candidate;
        }
    }
}

uint32_t extdataIdFor(uint64_t titleId) {
    const uint32_t low = static_cast<uint32_t>(titleId);
    switch (low) {
        case 0x00055E00: return 0x055D;  // Pokémon Y
        case 0x0011C400: return 0x11C5;  // Pokémon Omega Ruby
        case 0x00175E00: return 0x1648;  // Pokémon Moon
        case 0x00179600:
        case 0x00179800: return 0x1794;  // Fire Emblem Conquest SE NA
        case 0x00179700:
        case 0x0017A800: return 0x1795;  // Fire Emblem Conquest SE EU
        case 0x0012DD00:
        case 0x0012DE00: return 0x12DC;  // Fire Emblem If JP
        case 0x001B5100: return 0x1B50;  // Pokémon Ultra Moon
        default: return low >> 8;
    }
}

bool isSystemExcluded(uint64_t titleId) {
    const uint32_t low = static_cast<uint32_t>(titleId);
    const uint32_t high = static_cast<uint32_t>(titleId >> 32);
    switch (low) {
        case 0x00008602:  // instruction manuals
        case 0x00009202:
        case 0x00009B02:
        case 0x0000A402:
        case 0x0000AC02:
        case 0x0000B402:
        case 0x00021A00:  // garbage entry
            return true;
        default:
            break;
    }
    return high == 0x0004000E || high == 0x0004800F;
}

}  // namespace core
