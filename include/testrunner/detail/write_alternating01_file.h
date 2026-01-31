/**
 * @file write_alternating01_file.h
 * @brief Write a file consisting of bytes 0x55 (01010101...)
 * @author Sam Caldwell
 */
#pragma once

#include <cstdint>
#include <filesystem>

namespace crsce::testrunner::detail {
    /**
     * @name write_alternating01_file
     * @brief Create a file with the given number of bytes, each 0x55.
     * @param p Destination path.
     * @param bytes Number of bytes to write.
     */
    void write_alternating01_file(const std::filesystem::path &p, std::uint64_t bytes);
}

