/*
 * @file bithashbuffer_private_coverage_test.cpp
 * @brief Exercise byte flush behavior via public API only.
 */
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
/**
 * @file bithashbuffer_private_coverage_test.cpp
 * @brief Coverage of private helpers using friend exposure hooks.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
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
 * @name BitHashBufferPushAndFlushTest.PublicBehaviorMatchesExpectedRowHash
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */

TEST(BitHashBufferPushAndFlushTest, PublicBehaviorMatchesExpectedRowHash) {
    BitHashBuffer buf("seed");
    // Build one full row of 0xAB bytes via MSB-first bit pushes
    std::array<std::uint8_t, BitHashBuffer::kRowSize> row{};
    row.fill(0xAB); // 0b10101011 (MSB-first pushes: 1,0,1,0,1,0,1,1)
    const auto seed = buf.seedHash();
    for (std::size_t i = 0; i < BitHashBuffer::kRowSize; ++i) {
        buf.pushBit(true);
        buf.pushBit(false);
        buf.pushBit(true);
        buf.pushBit(false);
        buf.pushBit(true);
        buf.pushBit(false);
        buf.pushBit(true);
        buf.pushBit(true);
    }

    ASSERT_EQ(buf.count(), 1U);
    const auto expected = hash_row(seed, row);
    auto got = buf.popHash();
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(got.value(), expected);
}
