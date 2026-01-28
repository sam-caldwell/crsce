/**
 * @file write_random_file_at.h
 * @brief Create a file of fixed length with deterministic random bytes.
 */
#pragma once

#include <cstddef>
#include <fstream>
#include <ios>
#include <random>
#include <string>
#include <vector>

namespace crsce::testhelpers::tlt {
    /**
     * @name write_random_file_at
     * @brief Write a file of given size using bytes from rng (0..255).
     * @param path Destination file path.
     * @param bytes Number of bytes to write.
     * @param rng PRNG engine for deterministic data.
     * @return True on success; false on I/O failure.
     */
    inline bool write_random_file_at(const std::string &path, const std::size_t bytes, std::mt19937 &rng) {
        std::uniform_int_distribution<int> dist(0, 255);
        std::vector<char> buf(bytes);
        for (auto &c : buf) { c = static_cast<char>(dist(rng)); }
        std::ofstream f(path, std::ios::binary);
        if (!f.good()) { return false; }
        if (!buf.empty()) {
            f.write(buf.data(), static_cast<std::streamsize>(buf.size()));
        }
        return f.good();
    }
} // namespace crsce::testhelpers::tlt
