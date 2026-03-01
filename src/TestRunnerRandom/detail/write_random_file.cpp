/**
 * @file write_random_file.cpp
 * @brief Random file generation.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testRunnerRandom/detail/write_random_file.h"
#include <filesystem>
#include <fstream>
#include <ios>
#include <random>
#include <algorithm>
#include <cstddef>
#include <vector>
#include <cstdint>
#include <format>
#include <stdexcept>

namespace fs = std::filesystem;

namespace crsce::testrunner::detail {
    /**
     * @name write_random_file
     * @brief Create a file of random bytes of requested size.
     * @param p Destination path.
     * @param bytes Number of bytes to write.
     * @param rng PRNG engine.
     * @return void
     */
    void write_random_file(const fs::path &p, std::uint64_t bytes, std::mt19937_64 &rng) {
        std::ofstream os(p, std::ios::binary);
        if (!os) {
            throw std::runtime_error(
                std::format("failed to open input file for writing: {}", p.string()));
        }
        std::uniform_int_distribution<int> dist8(0, 255);
        constexpr std::size_t kChunk = 1U << 20U;
        std::vector<char> buf(kChunk);
        std::uint64_t remaining = bytes;
        while (remaining > 0) {
            const auto n = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, kChunk));
            for (std::size_t i = 0; i < n; ++i) { buf[i] = static_cast<char>(dist8(rng)); }
            os.write(buf.data(), static_cast<std::streamsize>(n));
            if (!os) {
                throw std::runtime_error(
                    std::format("failed writing random input to: {}", p.string()));
            }
            remaining -= n;
        }
    }
}
