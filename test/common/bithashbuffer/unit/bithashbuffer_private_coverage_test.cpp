/*
 * @file bithashbuffer_private_coverage_test.cpp
 * @brief Exercise byte flush behavior via public API only.
 */
#include <gtest/gtest.h>
#include <array>
#include <string>
#include <algorithm>
#include <iterator>
#include <cstddef>
#include <cstdint>

#include "common/BitHashBuffer.h"
#include "common/BitHashBuffer/detail/Sha256.h"

using crsce::common::BitHashBuffer;
using crsce::common::detail::sha256::sha256_digest;

namespace {
std::array<std::uint8_t, BitHashBuffer::kHashSize>
hash_row(const std::array<std::uint8_t, BitHashBuffer::kHashSize> &prev,
         const std::array<std::uint8_t, BitHashBuffer::kRowSize> &row) {
    std::array<std::uint8_t, BitHashBuffer::kHashSize + BitHashBuffer::kRowSize> buf{};
    std::ranges::copy(prev, buf.begin());
    std::ranges::copy(row, std::ranges::next(buf.begin(), static_cast<std::ptrdiff_t>(BitHashBuffer::kHashSize)));
    return sha256_digest(buf.data(), buf.size());
}
} // anonymous namespace

TEST(BitHashBufferPushAndFlushTest, PublicBehaviorMatchesExpectedRowHash) {
    BitHashBuffer buf("seed");
    // Partial bits should not produce any hash
    for (int i = 0; i < 3; ++i) {
        buf.pushBit(true);
    }
    EXPECT_EQ(buf.count(), 0U);

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
