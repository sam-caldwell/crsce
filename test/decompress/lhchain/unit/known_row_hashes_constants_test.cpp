/**
 * @file known_row_hashes_constants_test.cpp
 */
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string>

#include "common/BitHashBuffer/detail/Sha256.h"
#include "decompress/DeterministicElimination/KnownRowHashes.h"

using crsce::common::detail::sha256::sha256_digest;
using u8 = crsce::common::detail::sha256::u8;
namespace known = crsce::decompress::known_lh;

namespace {
    std::array<u8, 64> row_zeros() { return {}; }

    std::array<u8, 64> row_ones() {
        std::array<u8, 64> r{};
        r.fill(0xFF);
        r.back() = 0xFE;
        return r;
    }

    std::array<u8, 64> row_alt_0101() {
        std::array<u8, 64> r{};
        std::size_t byte_idx = 0;
        int bit_pos = 0;
        u8 curr = 0;
        for (std::size_t i = 0; i < 511; ++i) {
            if (const bool bit = (i % 2) != 0) {
                curr = static_cast<u8>(curr | static_cast<u8>(1U << (7 - bit_pos)));
            }
            ++bit_pos;
            if (bit_pos >= 8) {
                r.at(byte_idx++) = curr;
                curr = 0;
                bit_pos = 0;
            }
        }
        if (bit_pos != 0) { r.at(byte_idx) = curr; }
        return r;
    }

    std::array<u8, 64> row_alt_1010() {
        std::array<u8, 64> r{};
        std::size_t byte_idx = 0;
        int bit_pos = 0;
        u8 curr = 0;
        for (std::size_t i = 0; i < 511; ++i) {
            const bool bit = (i % 2) == 0;
            if (bit) { curr = static_cast<u8>(curr | static_cast<u8>(1U << (7 - bit_pos))); }
            ++bit_pos;
            if (bit_pos >= 8) {
                r.at(byte_idx++) = curr;
                curr = 0;
                bit_pos = 0;
            }
        }
        if (bit_pos != 0) { r.at(byte_idx) = curr; }
        return r;
    }
} // namespace

/**

 * @name KnownRowHashes.ConstantsMatchComputed

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(KnownRowHashes, ConstantsMatchComputed) { // NOLINT
    constexpr std::string seed = "CRSCE_v1_seed";
    const auto *seed_data = reinterpret_cast<const u8 *>(seed.data()); // NOLINT
    const auto seed_hash = sha256_digest(seed_data, seed.size());
    EXPECT_EQ(seed_hash, known::kSeedHash);

    auto row_hash = [&](const std::array<u8, 64> &row) {
        std::array<u8, 96> buf{};
        for (std::size_t i = 0; i < 32; ++i) { buf.at(i) = seed_hash.at(i); }
        for (std::size_t i = 0; i < 64; ++i) { buf.at(32 + i) = row.at(i); }
        return sha256_digest(buf.data(), buf.size());
    };

    EXPECT_EQ(row_hash(row_zeros()), known::kRowZerosDigest);
    EXPECT_EQ(row_hash(row_ones()), known::kRowOnesDigest);
    EXPECT_EQ(row_hash(row_alt_0101()), known::kRowAlt0101Digest);
    EXPECT_EQ(row_hash(row_alt_1010()), known::kRowAlt1010Digest);
}
