/**
 * @file write_zero_file.cpp
 * @brief Write a file consisting of zero bytes.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "testrunner/detail/write_zero_file.h"
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
     * @name write_zero_file
     * @brief Create a file with the given number of zero bytes.
     * @param p
     * @param bytes
     * @return void
     */
    void write_zero_file(const fs::path &p, const std::uint64_t bytes) {
        std::ofstream os(p, std::ios::binary);
        if (!os) { return; }
        constexpr std::size_t kChunk = 1U << 20U; // 1 MiB
        std::vector<char> buf(kChunk, 0);
        std::uint64_t remaining = bytes;
        while (remaining > 0) {
            const auto n = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, kChunk));
            os.write(buf.data(), static_cast<std::streamsize>(n));
            if (!os) { break; }
            remaining -= n;
        }
    }
}
