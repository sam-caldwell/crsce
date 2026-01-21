/**
 * @file print_known_hashes_test.cpp
 * @brief Utility test to print precomputed LH digests for canonical patterns.
 */
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <iostream>
#include <string>

#include "common/BitHashBuffer/detail/Sha256.h"

using crsce::common::detail::sha256::sha256_digest;
using u8 = crsce::common::detail::sha256::u8;

namespace {
std::array<u8, 32> seed_hash_for(const std::string &seed) {
    const auto *data = reinterpret_cast<const u8 *>(seed.data()); // NOLINT
    return sha256_digest(data, seed.size());
}

std::array<u8, 64> row_zeros() { return {}; }

std::array<u8, 64> row_ones() {
    std::array<u8, 64> r{};
    r.fill(0xFF);
    r.back() = 0xFE; // last pad bit 0
    return r;
}

std::array<u8, 64> row_alt_0101() {
    std::array<u8, 64> r{};
    std::size_t byte_idx = 0; int bit_pos = 0; u8 curr = 0;
    for (std::size_t i = 0; i < 511; ++i) {
        const bool bit = (i % 2) != 0;
        if (bit) {
            curr = static_cast<u8>(curr | static_cast<u8>(1U << (7 - bit_pos)));
        }
        ++bit_pos;
        if (bit_pos >= 8) {
            r.at(byte_idx++) = curr;
            curr = 0;
            bit_pos = 0;
        }
    }
    // pad 0
    if (bit_pos != 0) {
        r.at(byte_idx) = curr;
    }
    return r;
}

std::array<u8, 64> row_alt_1010() {
    std::array<u8, 64> r{};
    std::size_t byte_idx = 0; int bit_pos = 0; u8 curr = 0;
    for (std::size_t i = 0; i < 511; ++i) {
        const bool bit = (i % 2) == 0;
        if (bit) {
            curr = static_cast<u8>(curr | static_cast<u8>(1U << (7 - bit_pos)));
        }
        ++bit_pos;
        if (bit_pos >= 8) {
            r.at(byte_idx++) = curr;
            curr = 0;
            bit_pos = 0;
        }
    }
    // pad 0
    if (bit_pos != 0) {
        r.at(byte_idx) = curr;
    }
    return r;
}

std::string to_hex(const std::array<u8, 32> &d) {
    static constexpr std::array<char, 16> kHex{
        '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'
    };
    std::string out(64, '\0');
    for (std::size_t i = 0; i < d.size(); ++i) {
        const u8 b = d.at(i);
        out.at(2 * i) = kHex.at((b >> 4) & 0xF);
        out.at((2 * i) + 1) = kHex.at(b & 0xF);
    }
    return out;
}
} // namespace

TEST(PrintKnownHashes, Dump) { // NOLINT
    const std::string seed = "CRSCE_v1_seed";
    const auto seed_h = seed_hash_for(seed);
    auto hash = [&](const std::array<u8, 64> &row) {
        std::array<u8, 96> buf{};
        for (std::size_t i = 0; i < 32; ++i) {
            buf.at(i) = seed_h.at(i);
        }
        for (std::size_t i = 0; i < 64; ++i) {
            buf.at(32 + i) = row.at(i);
        }
        return sha256_digest(buf.data(), buf.size());
    };

    const auto d0 = hash(row_zeros());
    const auto d1 = hash(row_ones());
    const auto d2 = hash(row_alt_0101());
    const auto d3 = hash(row_alt_1010());
    std::cout << "seed: " << to_hex(seed_h) << "\n";
    std::cout << "zeros: " << to_hex(d0) << "\n";
    std::cout << "ones:  " << to_hex(d1) << "\n";
    std::cout << "0101:  " << to_hex(d2) << "\n";
    std::cout << "1010:  " << to_hex(d3) << "\n";
}
