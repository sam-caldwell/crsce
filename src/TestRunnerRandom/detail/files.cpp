/**
 * @file files.cpp
 * @brief Small file utilities for test runner.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/detail/files.h"
#include <fstream>
#include <filesystem>
#include <string_view>
#include <ios>

namespace fs = std::filesystem;

namespace crsce::testrunner::detail {
    /**
     * @name write_file_text
     * @brief Write the entire content to a file path.
     * @param p Destination path.
     * @param content Data to write.
     * @return true on success; false otherwise.
     */
    bool write_file_text(const fs::path &p, std::string_view content) {
        std::ofstream os(p, std::ios::binary);
        if (!os.is_open()) { return false; }
        os.write(content.data(), static_cast<std::streamsize>(content.size()));
        return static_cast<bool>(os);
    }

    // read_file_text moved to its own TU to satisfy ODPCPP
}
