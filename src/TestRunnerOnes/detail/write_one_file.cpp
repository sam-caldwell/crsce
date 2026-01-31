/**
 * @file write_one_file.cpp
 * @brief Write a file consisting of 0xFF bytes.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "testrunner/detail/write_one_file.h"
#include <filesystem>
#include <fstream>
#include <ios>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cstddef>

namespace fs = std::filesystem;

namespace crsce::testrunner::detail {
    /**
     * @name write_one_file
     * @brief Create a file with the given number of 0xFF bytes.
     * @param p
     * @param bytes
     * @return void
     */
    void write_one_file(const fs::path &p, const std::uint64_t bytes) {
        std::ofstream os(p, std::ios::binary);
        if (!os) { return; }
        constexpr std::size_t kChunk = 1U << 20U; // 1 MiB
        std::vector<unsigned char> buf(kChunk, 0xFFU);
        std::uint64_t remaining = bytes;
        while (remaining > 0) {
            const auto n = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, kChunk));
            os.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(n)); // NOLINT
            if (!os) { break; }
            remaining -= n;
        }
    }
}

