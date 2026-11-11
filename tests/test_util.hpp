#pragma once

#include <fstream>
#include <sstream>
#include <string>

// Reads a test fixture relative to the tests/ directory (make -C tests runs there).
inline std::string readFile(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
