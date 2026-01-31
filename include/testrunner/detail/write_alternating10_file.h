/**
 * @file write_alternating10_file.h
 * @brief Write a file consisting of bytes 0xAA (10101010...)
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <filesystem>

namespace crsce::testrunner::detail {
    /**
     * @name write_alternating10_file
     * @brief Create a file with the given number of bytes, each 0xAA.
     * @param p Destination path.
     * @param bytes Number of bytes to write.
     */
    void write_alternating10_file(const std::filesystem::path &p, std::uint64_t bytes);
}
