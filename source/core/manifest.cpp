#include "core/manifest.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <tuple>

namespace core {

namespace {

constexpr std::string_view kHeader = "3dsavesync-manifest 1";
constexpr std::string_view kTitlePrefix = "title ";

const char* partName(Part p) {
    return p == Part::Save ? "save" : "extdata";
}

auto sortKey(const FileEntry& f) {
    return std::tie(f.part, f.path, f.size, f.sha256);
}

std::vector<FileEntry> sorted(std::vector<FileEntry> files) {
    std::sort(files.begin(), files.end(),
              [](const FileEntry& a, const FileEntry& b) { return sortKey(a) < sortKey(b); });
    return files;
}

auto dirKey(const DirEntry& d) {
    return std::tie(d.part, d.path);
}

std::vector<DirEntry> sortedDirs(std::vector<DirEntry> dirs) {
    std::sort(dirs.begin(), dirs.end(), [](const DirEntry& a, const DirEntry& b) { return dirKey(a) < dirKey(b); });
    return dirs;
}

std::optional<DirEntry> parseDirLine(std::string_view line) {
    const auto tab = line.find('\t');
    if (tab == std::string_view::npos || tab + 1 >= line.size()) {
        return std::nullopt;
    }
    const std::string_view kind = line.substr(0, tab);
    if (kind == "savedir") {
        return DirEntry{Part::Save, std::string(line.substr(tab + 1))};
    }
    if (kind == "extdatadir") {
        return DirEntry{Part::Extdata, std::string(line.substr(tab + 1))};
    }
    return std::nullopt;
}

bool isLowerHex(std::string_view s) {
    return std::all_of(s.begin(), s.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

template <typename T>
bool parseNumber(std::string_view s, int base, T& out) {
    if (s.empty()) {
        return false;
    }
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out, base);
    return ec == std::errc() && ptr == s.data() + s.size();
}

std::optional<FileEntry> parseFileLine(std::string_view line) {
    std::string_view fields[3];
    for (auto& field : fields) {
        const auto tab = line.find('\t');
        if (tab == std::string_view::npos) {
            return std::nullopt;
        }
        field = line.substr(0, tab);
        line.remove_prefix(tab + 1);
    }
    FileEntry entry;
    if (fields[0] == "save") {
        entry.part = Part::Save;
    } else if (fields[0] == "extdata") {
        entry.part = Part::Extdata;
    } else {
        return std::nullopt;
    }
    if (!parseNumber(fields[1], 10, entry.size)) {
        return std::nullopt;
    }
    if (fields[2].size() != 64 || !isLowerHex(fields[2])) {
        return std::nullopt;
    }
    entry.sha256 = std::string(fields[2]);
    if (line.empty()) {
        return std::nullopt;
    }
    entry.path = std::string(line);
    return entry;
}

}  // namespace

std::string serializeManifest(const Manifest& m) {
    char title[32];
    std::snprintf(title, sizeof(title), "%016llX", static_cast<unsigned long long>(m.titleId));
    std::string out(kHeader);
    out += '\n';
    out += kTitlePrefix;
    out += title;
    out += '\n';
    for (const FileEntry& f : sorted(m.files)) {
        out += partName(f.part);
        out += '\t';
        out += std::to_string(f.size);
        out += '\t';
        out += f.sha256;
        out += '\t';
        out += f.path;
        out += '\n';
    }
    for (const DirEntry& d : sortedDirs(m.dirs)) {
        out += d.part == Part::Save ? "savedir" : "extdatadir";
        out += '\t';
        out += d.path;
        out += '\n';
    }
    return out;
}

std::optional<Manifest> parseManifest(std::string_view text) {
    Manifest m;
    int lineNo = 0;
    while (!text.empty()) {
        const auto nl = text.find('\n');
        std::string_view line = text.substr(0, nl);
        text.remove_prefix(nl == std::string_view::npos ? text.size() : nl + 1);
        ++lineNo;
        if (lineNo == 1) {
            if (line != kHeader) {
                return std::nullopt;
            }
        } else if (lineNo == 2) {
            if (!line.starts_with(kTitlePrefix) || !parseNumber(line.substr(kTitlePrefix.size()), 16, m.titleId)) {
                return std::nullopt;
            }
        } else if (line.starts_with("savedir\t") || line.starts_with("extdatadir\t")) {
            auto dir = parseDirLine(line);
            if (!dir) {
                return std::nullopt;
            }
            m.dirs.push_back(std::move(*dir));
        } else if (!line.empty()) {
            auto entry = parseFileLine(line);
            if (!entry) {
                return std::nullopt;
            }
            m.files.push_back(std::move(*entry));
        }
    }
    if (lineNo < 2) {
        return std::nullopt;
    }
    return m;
}

bool sameContent(const Manifest& a, const Manifest& b) {
    if (a.files.size() != b.files.size() || a.dirs.size() != b.dirs.size()) {
        return false;
    }
    const auto sa = sorted(a.files);
    const auto sb = sorted(b.files);
    const auto da = sortedDirs(a.dirs);
    const auto db = sortedDirs(b.dirs);
    return std::equal(sa.begin(), sa.end(), sb.begin(),
                      [](const FileEntry& x, const FileEntry& y) { return sortKey(x) == sortKey(y); }) &&
           std::equal(da.begin(), da.end(), db.begin(),
                      [](const DirEntry& x, const DirEntry& y) { return dirKey(x) == dirKey(y); });
}

bool hasPart(const Manifest& m, Part p) {
    return std::any_of(m.files.begin(), m.files.end(), [p](const FileEntry& f) { return f.part == p; });
}

}  // namespace core
