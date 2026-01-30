/**
 * @file read_file_text.h
 * @brief Read entire file contents into a string.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <filesystem>
#include <string>

namespace crsce::testrunner::detail {
    /**
     * @name read_file_text
     * @brief Read entire file into a string.
     * @param p Source file path.
     * @return File contents or empty string on failure.
     */
    std::string read_file_text(const std::filesystem::path &p);
}
