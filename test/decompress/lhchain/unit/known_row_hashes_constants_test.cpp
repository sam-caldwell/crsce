/**
 * @file known_row_hashes_constants_test.cpp
 */
#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include "common/BitHashBuffer/detail/Sha256Types.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"

using crsce::common::detail::sha256::sha256_digest;
using u8 = crsce::common::detail::sha256::u8;
namespace known {}

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
 * @name RowPacking.DigestsForCanonicalRows
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(RowPacking, DigestsForCanonicalRows) { // NOLINT
    // Per-row hashing: digest = sha256(row64)
    const auto d0 = sha256_digest(row_zeros().data(), row_zeros().size());
    const auto d1 = sha256_digest(row_ones().data(), row_ones().size());
    const auto dA = sha256_digest(row_alt_0101().data(), row_alt_0101().size());
    const auto dB = sha256_digest(row_alt_1010().data(), row_alt_1010().size());
    // Basic invariants: digests are 32 bytes and distinct across patterns
    EXPECT_EQ(d0.size(), 32U);
    EXPECT_EQ(d1.size(), 32U);
    EXPECT_EQ(dA.size(), 32U);
    EXPECT_EQ(dB.size(), 32U);
    EXPECT_NE(d0, d1);
    EXPECT_NE(d0, dA);
    EXPECT_NE(d0, dB);
    EXPECT_NE(d1, dA);
    EXPECT_NE(d1, dB);
    EXPECT_NE(dA, dB);
}
