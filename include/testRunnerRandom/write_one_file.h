/**
 * @file write_one_file.h
 * @brief Write a file consisting of the specified number of 0xFF bytes.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <filesystem>

namespace crsce::testrunner::detail {
    /**
     * @name write_one_file
     * @brief Create a file with the given number of 0xFF bytes.
     * @param p Destination path.
     * @param bytes Number of 0xFF bytes to write.
     */
    void write_one_file(const std::filesystem::path &p, std::uint64_t bytes);
}
