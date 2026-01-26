/**
 * @file bithashbuffer_finalize_row_private_test.cpp
 * @brief FinalizeRow private path is exercised via test exposure.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <iterator>

#include "../../../../include/common/BitHashBuffer/BitHashBuffer.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"

using crsce::common::BitHashBuffer;
using crsce::common::detail::sha256::sha256_digest;

namespace {
    std::array<std::uint8_t, BitHashBuffer::kHashSize>
    hash_row(const std::array<std::uint8_t, BitHashBuffer::kHashSize> &prev,
             const std::array<std::uint8_t, BitHashBuffer::kRowSize> &row) {
        std::array<std::uint8_t, BitHashBuffer::kHashSize + BitHashBuffer::kRowSize>
                buf{};
        std::ranges::copy(prev, buf.begin());
        std::ranges::copy(
            row, std::ranges::next(buf.begin(), static_cast<std::ptrdiff_t>(
                                       BitHashBuffer::kHashSize)));
        return sha256_digest(buf.data(), buf.size());
    }
} // anonymous namespace

/**
 * @name BitHashBufferFinalizeRowPathsTest.FinalizeRowEarlyAndFullPaths
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(BitHashBufferFinalizeRowPathsTest, FinalizeRowEarlyAndFullPaths) {
    // Early path: push fewer than 64 bytes; no hash produced
    {
        BitHashBuffer buf("zz");
        const auto before = buf.count();
        // Push 10 full bytes of zeros (80 bits)
        for (int i = 0; i < 10; ++i) {
            for (int b = 0; b < 8; ++b) {
                buf.pushBit(false);
            }
        }
        EXPECT_EQ(buf.count(), before);
    }

    // Full path: push exactly 64 bytes of 0x3C, expect a single hash
    {
        BitHashBuffer buf("zz");
        const auto seed = buf.seedHash();
        std::array<std::uint8_t, BitHashBuffer::kRowSize> row{};
        row.fill(0x3C);
        // 0x3C = 0b00111100; MSB-first: 0,0,1,1,1,1,0,0
        for (std::size_t i = 0; i < BitHashBuffer::kRowSize; ++i) {
            buf.pushBit(false);
            buf.pushBit(false);
            buf.pushBit(true);
            buf.pushBit(true);
            buf.pushBit(true);
            buf.pushBit(true);
            buf.pushBit(false);
            buf.pushBit(false);
        }
        ASSERT_EQ(buf.count(), 1U);
        const auto expected = hash_row(seed, row);
        auto got = buf.popHash();
        ASSERT_TRUE(got.has_value());
        EXPECT_EQ(got.value(), expected);
    }
}
