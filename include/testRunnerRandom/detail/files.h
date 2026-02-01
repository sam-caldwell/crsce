/**
 * @file files.h
 * @brief Small file utilities for test runner.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace crsce::testrunner::detail {
    /**
     * @name write_file_text
     * @brief Write text content to a file path (binary-safe).
     * @param p Destination file path.
     * @param content Data to write.
     * @return true on success; false otherwise.
     */
    bool write_file_text(const std::filesystem::path &p, std::string_view content);

}
