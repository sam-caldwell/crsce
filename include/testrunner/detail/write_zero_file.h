/**
 * @file write_zero_file.h
 * @brief Write a file consisting of the specified number of zero bytes.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <filesystem>

namespace crsce::testrunner::detail {
    /**
     * @name write_zero_file
     * @brief Create a file with the given number of zero bytes.
     * @param p Destination path.
     * @param bytes Number of zero bytes to write.
     */
    void write_zero_file(const std::filesystem::path &p, std::uint64_t bytes);
}
