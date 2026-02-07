/**
 * @file bithashbuffer_finalize_row_private_test.cpp
 * @brief FinalizeRow private path is exercised via test exposure.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>

#include "../../../../include/common/BitHashBuffer/BitHashBuffer.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"

using crsce::common::BitHashBuffer;
using crsce::common::detail::sha256::sha256_digest;

namespace {} // anonymous namespace

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
        const auto expected = sha256_digest(row.data(), row.size());
        auto got = buf.popHash();
        ASSERT_TRUE(got.has_value());
        EXPECT_EQ(got.value(), expected);
    }
}
