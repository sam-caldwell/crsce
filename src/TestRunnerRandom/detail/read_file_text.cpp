/**
 * @file read_file_text.cpp
 * @brief Read entire file contents to a string.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/detail/read_file_text.h"
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace crsce::testrunner::detail {
    /**
     * @name read_file_text
     * @brief Read entire file contents to a string; returns empty on failure.
     * @param p Source path.
     * @return File contents or empty string.
     */
    std::string read_file_text(const fs::path &p) {
        const std::ifstream is(p, std::ios::binary); // NOLINT(misc-const-correctness)
        if (!is) { return {}; }
        std::ostringstream ss; ss << is.rdbuf();
        return ss.str();
    }
}
