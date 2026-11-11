#include "core/propfind.hpp"

#include <charconv>
#include <string>

#include <tinyxml2.h>

#include "core/url.hpp"

namespace core {

namespace {

std::string_view localName(const tinyxml2::XMLElement* el) {
    std::string_view name = el->Name();
    const auto colon = name.find(':');
    return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

// Depth-first search for the first descendant with the given local name.
const tinyxml2::XMLElement* findDescendant(const tinyxml2::XMLElement* el, std::string_view name) {
    for (auto* child = el->FirstChildElement(); child != nullptr; child = child->NextSiblingElement()) {
        if (localName(child) == name) {
            return child;
        }
        if (auto* found = findDescendant(child, name)) {
            return found;
        }
    }
    return nullptr;
}

// First descendant with the given local name whose text is not empty.
const char* findNonEmptyText(const tinyxml2::XMLElement* el, std::string_view name) {
    for (auto* child = el->FirstChildElement(); child != nullptr; child = child->NextSiblingElement()) {
        if (localName(child) == name && child->GetText() != nullptr && *child->GetText() != '\0') {
            return child->GetText();
        }
        if (const char* found = findNonEmptyText(child, name)) {
            return found;
        }
    }
    return nullptr;
}

std::string stripTrailingSlash(std::string path) {
    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }
    return path;
}

std::string normalizeHref(std::string_view href, std::string_view basePath) {
    const auto scheme = href.find("://");
    if (scheme != std::string_view::npos) {
        const auto pathStart = href.find('/', scheme + 3);
        href = pathStart == std::string_view::npos ? std::string_view("/") : href.substr(pathStart);
    }
    std::string path = decodePercent(href);
    if (!basePath.empty() && path.starts_with(basePath) &&
        (path.size() == basePath.size() || path[basePath.size()] == '/')) {
        path.erase(0, basePath.size());
        if (path.empty()) {
            path = "/";
        }
    }
    return stripTrailingSlash(std::move(path));
}

}  // namespace

std::expected<std::vector<DavEntry>, Error> parsePropfind(std::string_view xml, std::string_view basePath,
                                                          std::string_view requestPath) {
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.data(), xml.size()) != tinyxml2::XML_SUCCESS) {
        return std::unexpected(Error{ErrorKind::Protocol, "Invalid PROPFIND response"});
    }
    const tinyxml2::XMLElement* root = doc.RootElement();
    if (root == nullptr || localName(root) != "multistatus") {
        return std::unexpected(Error{ErrorKind::Protocol, "PROPFIND response has no multistatus"});
    }

    const std::string self = stripTrailingSlash(std::string(requestPath));
    std::vector<DavEntry> entries;
    for (auto* response = root->FirstChildElement(); response != nullptr; response = response->NextSiblingElement()) {
        if (localName(response) != "response") {
            continue;
        }
        const tinyxml2::XMLElement* href = findDescendant(response, "href");
        if (href == nullptr || href->GetText() == nullptr) {
            return std::unexpected(Error{ErrorKind::Protocol, "PROPFIND response without href"});
        }
        const std::string path = normalizeHref(href->GetText(), basePath);
        if (path == self) {
            continue;
        }
        DavEntry entry;
        entry.name = path.substr(path.rfind('/') + 1);
        entry.isDir = findDescendant(response, "collection") != nullptr;
        entry.size = 0;
        if (const char* length = findNonEmptyText(response, "getcontentlength")) {
            const std::string_view text(length);
            std::from_chars(text.data(), text.data() + text.size(), entry.size);
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

}  // namespace core
