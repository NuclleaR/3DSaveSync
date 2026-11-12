#include <sys/stat.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "doctest.h"
#include "platform/sd_fs.hpp"

namespace {

// A fresh directory under the system temp folder, standing in for "sdmc:".
std::string tempRoot() {
    std::string pattern = "/tmp/3dsavesync-sdfs-XXXXXX";
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    return pattern;
}

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool isDir(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

}  // namespace

TEST_CASE("SdFs creates nested folders and writes files") {
    const std::string root = tempRoot();
    platform::SdFs fs(root);
    REQUIRE(fs.makeDirs("/3ds/Checkpoint/saves/0x00001 Game/20261112-194000"));
    CHECK(isDir(root + "/3ds/Checkpoint/saves/0x00001 Game/20261112-194000"));
    CHECK(fs.makeDirs("/3ds/Checkpoint/saves"));  // existing folders are fine

    auto w = fs.create("/3ds/Checkpoint/saves/0x00001 Game/20261112-194000/main");
    REQUIRE(w);
    REQUIRE((*w)->write("abc", 3));
    REQUIRE((*w)->write("def", 3));
    REQUIRE((*w)->close());
    CHECK(slurp(root + "/3ds/Checkpoint/saves/0x00001 Game/20261112-194000/main") == "abcdef");
    REQUIRE(fs.removeTree("/3ds"));
}

TEST_CASE("SdFs lists only folders, and a missing folder is empty") {
    const std::string root = tempRoot();
    platform::SdFs fs(root);
    REQUIRE(fs.makeDirs("/t/a"));
    REQUIRE(fs.makeDirs("/t/b-2"));
    auto w = fs.create("/t/file");
    REQUIRE(w);
    REQUIRE((*w)->close());
    auto dirs = fs.listDirs("/t");
    REQUIRE(dirs);
    std::sort(dirs->begin(), dirs->end());
    CHECK(*dirs == std::vector<std::string>{"a", "b-2"});
    auto missing = fs.listDirs("/missing");
    REQUIRE(missing);
    CHECK(missing->empty());
    auto notAFolder = fs.listDirs("/t/file");
    CHECK_FALSE(notAFolder);
    REQUIRE(fs.removeTree("/t"));
}

TEST_CASE("SdFs removes a whole tree, and a missing one is fine") {
    const std::string root = tempRoot();
    platform::SdFs fs(root);
    REQUIRE(fs.makeDirs("/x/y/z"));
    auto w = fs.create("/x/y/z/f");
    REQUIRE(w);
    REQUIRE((*w)->write("1", 1));
    REQUIRE((*w)->close());
    REQUIRE(fs.removeTree("/x"));
    CHECK_FALSE(isDir(root + "/x"));
    CHECK(fs.removeTree("/x"));
}

TEST_CASE("SdFs create fails in a missing folder") {
    const std::string root = tempRoot();
    platform::SdFs fs(root);
    auto w = fs.create("/nope/f");
    REQUIRE_FALSE(w);
    CHECK(w.error().kind == core::ErrorKind::Io);
}
