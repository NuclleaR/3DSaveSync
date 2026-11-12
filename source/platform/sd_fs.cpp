#include "platform/sd_fs.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

namespace platform {

namespace {

constexpr size_t kWriteBuffer = 64 * 1024;

core::Error ioError(const std::string& what, int err) {
    return core::Error{core::ErrorKind::Io, what + ": " + std::strerror(err)};
}

bool isDirectory(const std::string& full) {
    struct stat st{};
    return stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// Names in a folder, without "." and ".."; {name, is a folder}. A missing
// folder has none; any other failure is an error.
std::expected<std::vector<std::pair<std::string, bool>>, core::Error> entries(const std::string& full) {
    std::vector<std::pair<std::string, bool>> out;
    DIR* dir = opendir(full.c_str());
    if (dir == nullptr) {
        if (errno == ENOENT) {
            return out;
        }
        return std::unexpected(ioError("Listing " + full, errno));
    }
    while (const dirent* e = readdir(dir)) {
        const std::string name = e->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        out.emplace_back(name, isDirectory(full + "/" + name));
    }
    closedir(dir);
    return out;
}

class FileWriter : public core::LocalWriter {
public:
    FileWriter(std::FILE* f, std::string path) : f_(f), path_(std::move(path)), buf_(kWriteBuffer) {
        std::setvbuf(f_, buf_.data(), _IOFBF, buf_.size());
    }
    ~FileWriter() override {
        if (f_ != nullptr) {
            std::fclose(f_);
        }
    }
    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;

    std::expected<void, core::Error> write(const char* data, size_t len) override {
        if (std::fwrite(data, 1, len, f_) != len) {
            return std::unexpected(ioError("Writing " + path_, errno));
        }
        return {};
    }

    std::expected<void, core::Error> close() override {
        std::FILE* f = std::exchange(f_, nullptr);
        if (f == nullptr || std::fclose(f) != 0) {
            return std::unexpected(ioError("Closing " + path_, errno));
        }
        return {};
    }

private:
    std::FILE* f_;
    std::string path_;
    std::vector<char> buf_;  // must outlive f_
};

std::expected<void, core::Error> removeFull(const std::string& full) {
    struct stat st{};
    if (stat(full.c_str(), &st) != 0) {
        if (errno == ENOENT) {
            return {};
        }
        return std::unexpected(ioError("Checking " + full, errno));
    }
    if (!S_ISDIR(st.st_mode)) {
        if (unlink(full.c_str()) != 0) {
            return std::unexpected(ioError("Deleting " + full, errno));
        }
        return {};
    }
    auto children = entries(full);
    if (!children) {
        return std::unexpected(children.error());
    }
    for (const auto& [name, dir] : *children) {
        if (auto r = removeFull(full + "/" + name); !r) {
            return r;
        }
    }
    if (rmdir(full.c_str()) != 0) {
        return std::unexpected(ioError("Deleting " + full, errno));
    }
    return {};
}

}  // namespace

SdFs::SdFs(std::string prefix) : prefix_(std::move(prefix)) {}

std::expected<std::vector<std::string>, core::Error> SdFs::listDirs(const std::string& path) {
    auto found = entries(prefix_ + path);
    if (!found) {
        return std::unexpected(found.error());
    }
    std::vector<std::string> out;
    for (auto& [name, dir] : *found) {
        if (dir) {
            out.push_back(std::move(name));
        }
    }
    return out;
}

std::expected<void, core::Error> SdFs::makeDirs(const std::string& path) {
    for (size_t pos = path.find('/', 1);; pos = path.find('/', pos + 1)) {
        const std::string full = prefix_ + path.substr(0, pos);
        if (mkdir(full.c_str(), 0777) != 0 && errno != EEXIST) {
            return std::unexpected(ioError("Creating " + path.substr(0, pos), errno));
        }
        if (pos == std::string::npos) {
            return {};
        }
    }
}

std::expected<std::unique_ptr<core::LocalWriter>, core::Error> SdFs::create(const std::string& path) {
    std::FILE* f = std::fopen((prefix_ + path).c_str(), "wb");
    if (f == nullptr) {
        return std::unexpected(ioError("Creating " + path, errno));
    }
    return std::unique_ptr<core::LocalWriter>(new FileWriter(f, path));
}

std::expected<void, core::Error> SdFs::removeTree(const std::string& path) {
    return removeFull(prefix_ + path);
}

}  // namespace platform
