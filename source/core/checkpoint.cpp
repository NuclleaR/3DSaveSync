/*
 *   This file is part of 3DSaveSync, licensed under the GNU General Public
 *   License version 3 or (at your option) any later version. See LICENSE.
 *
 *   Portions are adapted from Checkpoint
 *   (https://github.com/BernardoGiordano/Checkpoint):
 *     - extdataIdFor() and isSystemExcluded(): the id tables of
 *       TitleQuirks::extdataIdFor / TitleQuirks::isSystemExcluded,
 *       3ds/source/titlequirks.cpp;
 *     - sanitizeName(), titleFolderName(), versionName(): reproduce the
 *       behaviour of StringUtils::removeForbiddenCharacters,
 *       Paths::ctrFolderName and DateTime::dateTimeStr.
 *   Modified for 3DSaveSync: rewritten as free functions in namespace core,
 *   without libctru types. This is not the original Checkpoint code.
 *
 *   Original notice:
 *     This file is part of Checkpoint
 *     Copyright (C) 2017-2026 Bernardo Giordano, FlagBrew
 *
 *     This program is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     Additional Terms 7.b and 7.c of GPLv3 apply to this file:
 *         * Requiring preservation of specified reasonable legal notices or
 *           author attributions in that material or in the Appropriate Legal
 *           Notices displayed by works containing it.
 *         * Prohibiting misrepresentation of the origin of that material,
 *           or requiring that modified versions of such material be marked in
 *           reasonable ways as different from the original version.
 */

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

std::string versionLabel(std::string_view version) {
    constexpr size_t kLen = 15;  // YYYYMMDD-HHMMSS
    auto digits = [&](size_t from, size_t to) {
        return std::all_of(version.begin() + from, version.begin() + to, [](char c) { return c >= '0' && c <= '9'; });
    };
    if (version.size() < kLen || version[8] != '-' || !digits(0, 8) || !digits(9, kLen)) {
        return std::string(version);
    }
    std::string out;
    out.append(version.substr(0, 4)).append("-").append(version.substr(4, 2)).append("-");
    out.append(version.substr(6, 2)).append(" ").append(version.substr(9, 2)).append(":");
    out.append(version.substr(11, 2));
    const std::string_view rest = version.substr(kLen);
    if (rest.empty()) {
        return out;
    }
    if (rest.size() >= 2 && rest[0] == '-' && digits(kLen + 1, version.size())) {
        return out + " (" + std::string(rest.substr(1)) + ")";
    }
    return std::string(version);
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
