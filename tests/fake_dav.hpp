#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/dav.hpp"

// In-memory WebDAV tree with failure injection. Paths are absolute and have
// no trailing '/'. Behaves like Koofr: PUT/MKCOL/MOVE need an existing parent
// (409 otherwise), MOVE onto an existing path is a Conflict, DELETE of a
// missing path succeeds.
class FakeDav : public core::Dav {
public:
    int deletes = 0;

    // The nth call (1-based) of `method` whose path starts with `pathPrefix`
    // fails with `kind` and changes nothing. Methods: PROPFIND, MKCOL, PUT, GET, MOVE, DELETE.
    void failOn(std::string method, std::string pathPrefix, int nth, core::ErrorKind kind) {
        failures_.push_back({std::move(method), std::move(pathPrefix), nth, kind, 0});
    }

    // Serve different bytes for this file on GET (simulates a corrupted download).
    void corrupt(const std::string& path) { corrupted_.push_back(path); }

    void mkfile(const std::string& path, const std::string& content = "x") {
        mkdirs(parent(path));
        nodes_[path] = {false, content};
    }
    bool exists(const std::string& path) const { return path == "/" || nodes_.count(path) != 0; }
    bool isFile(const std::string& path) const {
        auto it = nodes_.find(path);
        return it != nodes_.end() && !it->second.isDir;
    }
    std::string content(const std::string& path) const { return nodes_.at(path).content; }

    std::expected<std::vector<core::DavEntry>, core::Error> list(const std::string& path) override {
        if (auto e = fail("PROPFIND", path)) return std::unexpected(*e);
        if (!isDir(path)) return std::unexpected(core::Error{core::ErrorKind::NotFound, path});
        std::vector<core::DavEntry> out;
        const std::string prefix = path == "/" ? "/" : path + "/";
        for (const auto& [p, n] : nodes_) {
            if (p.size() > prefix.size() && p.starts_with(prefix) && p.find('/', prefix.size()) == std::string::npos) {
                out.push_back({p.substr(prefix.size()), n.isDir, n.content.size()});
            }
        }
        return out;
    }

    std::expected<void, core::Error> makeDirs(const std::string& path) override {
        if (auto e = fail("MKCOL", path)) return std::unexpected(*e);
        if (!mkdirs(path)) return std::unexpected(core::Error{core::ErrorKind::Conflict, path});
        return {};
    }

    std::expected<void, core::Error> put(const std::string& path, core::BodySource body) override {
        if (auto e = fail("PUT", path)) return std::unexpected(*e);
        if (!isDir(parent(path))) return std::unexpected(core::Error{core::ErrorKind::Conflict, path});
        std::string data;
        char buf[3];
        for (;;) {
            auto n = body.read(buf, sizeof(buf));
            if (!n) return std::unexpected(n.error());
            if (*n == 0) break;
            data.append(buf, *n);
        }
        nodes_[path] = {false, std::move(data)};
        return {};
    }

    std::expected<std::string, core::Error> getText(const std::string& path) override {
        if (auto e = fail("GET", path)) return std::unexpected(*e);
        if (!isFile(path)) return std::unexpected(core::Error{core::ErrorKind::NotFound, path});
        return served(path);
    }

    std::expected<void, core::Error> get(const std::string& path, core::BodySink& sink) override {
        if (auto e = fail("GET", path)) return std::unexpected(*e);
        if (!isFile(path)) return std::unexpected(core::Error{core::ErrorKind::NotFound, path});
        const std::string data = served(path);
        return sink.write(data.data(), data.size());
    }

    std::expected<void, core::Error> move(const std::string& from, const std::string& to) override {
        if (auto e = fail("MOVE", from)) return std::unexpected(*e);
        if (!exists(from) || from == "/") return std::unexpected(core::Error{core::ErrorKind::NotFound, from});
        if (exists(to)) return std::unexpected(core::Error{core::ErrorKind::Conflict, to});
        if (!isDir(parent(to))) return std::unexpected(core::Error{core::ErrorKind::Conflict, to});
        std::map<std::string, Node> moved;
        for (auto it = nodes_.begin(); it != nodes_.end();) {
            if (inSubtree(it->first, from)) {
                moved[to + it->first.substr(from.size())] = it->second;
                it = nodes_.erase(it);
            } else {
                ++it;
            }
        }
        nodes_.insert(moved.begin(), moved.end());
        return {};
    }

    std::expected<void, core::Error> remove(const std::string& path) override {
        ++deletes;
        if (auto e = fail("DELETE", path)) return std::unexpected(*e);
        for (auto it = nodes_.begin(); it != nodes_.end();) {
            it = inSubtree(it->first, path) ? nodes_.erase(it) : std::next(it);
        }
        return {};
    }

private:
    struct Node {
        bool isDir;
        std::string content;
    };
    struct Failure {
        std::string method;
        std::string prefix;
        int nth;
        core::ErrorKind kind;
        int seen;
    };

    static std::string parent(const std::string& path) {
        const auto slash = path.rfind('/');
        return slash == 0 || slash == std::string::npos ? "/" : path.substr(0, slash);
    }
    static bool inSubtree(const std::string& p, const std::string& root) {
        return p == root || p.starts_with(root + "/");
    }
    bool isDir(const std::string& path) const {
        if (path == "/") return true;
        auto it = nodes_.find(path);
        return it != nodes_.end() && it->second.isDir;
    }
    bool mkdirs(const std::string& path) {
        if (path == "/") return true;
        if (!mkdirs(parent(path))) return false;
        auto it = nodes_.find(path);
        if (it != nodes_.end()) return it->second.isDir;
        nodes_[path] = {true, ""};
        return true;
    }
    std::optional<core::Error> fail(const std::string& method, const std::string& path) {
        for (auto& f : failures_) {
            if (f.method == method && path.starts_with(f.prefix) && ++f.seen == f.nth) {
                return core::Error{f.kind, method + " " + path};
            }
        }
        return std::nullopt;
    }
    std::string served(const std::string& path) const {
        std::string data = nodes_.at(path).content;
        for (const auto& c : corrupted_) {
            if (c == path) data += "!";
        }
        return data;
    }

    std::map<std::string, Node> nodes_;
    std::vector<Failure> failures_;
    std::vector<std::string> corrupted_;
};
