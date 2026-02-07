/**
 * @file write_zero_file.cpp
 * @brief Definition of write_zero_file utility for test runners.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testRunnerRandom/detail/write_zero_file.h"

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <array>
#include <algorithm>

namespace crsce::testrunner::detail {

/**
 * @name write_zero_file
 * @brief Create a file with the given number of zero bytes.
 * @param p Destination path.
 * @param bytes Number of zero bytes to write.
 */
void write_zero_file(const std::filesystem::path &p, std::uint64_t bytes) {
    std::ofstream os(p, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!os.is_open()) { return; }
    constexpr std::size_t kChunk = 64 * 1024;
    std::array<char, kChunk> buf{};
    std::uint64_t remaining = bytes;
    while (remaining > 0) {
        const auto take64 = std::min<std::uint64_t>(remaining, static_cast<std::uint64_t>(kChunk));
        const auto take = static_cast<std::size_t>(take64);
        os.write(buf.data(), static_cast<std::streamsize>(take));
        remaining -= static_cast<std::uint64_t>(take);
    }
    os.flush();
}

} // namespace crsce::testrunner::detail
