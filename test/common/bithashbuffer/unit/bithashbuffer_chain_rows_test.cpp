/**
 * @file bithashbuffer_chain_rows_test.cpp
 * @brief BitHashBuffer chains multiple rows and produces expected digests.
 * @copyright (c) 2026 Sam Caldwell.
 * See LICENSE.txt for details.
 */
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <iterator>
#include <string>

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
 * @name BitHashBufferChainRowsTest.TwoRowsChainingAndPopFifo
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(BitHashBufferChainRowsTest, TwoRowsChainingAndPopFifo) {
    const std::string seed = "seed"; // simple case, single-block padding
    BitHashBuffer buf(seed);
    const auto seed_h = buf.seedHash();

    // Row0: 0x80 bytes (push 1 then seven 0s repeatedly)
    for (std::size_t i = 0; i < BitHashBuffer::kRowSize; ++i) {
        buf.pushBit(true);
        for (int b = 0; b < 7; ++b) {
            buf.pushBit(false);
        }
    }
    // Row1: 0x01 bytes (push seven 0s then a 1 repeatedly)
    for (std::size_t i = 0; i < BitHashBuffer::kRowSize; ++i) {
        for (int b = 0; b < 7; ++b) {
            buf.pushBit(false);
        }
        buf.pushBit(true);
    }

    // Expect two hashes available
    ASSERT_EQ(buf.count(), 2U);

    // Build expected rows and chained hashes
    std::array<std::uint8_t, BitHashBuffer::kRowSize> row0{};
    row0.fill(0x80);
    std::array<std::uint8_t, BitHashBuffer::kRowSize> row1{};
    row1.fill(0x01);
    auto h0 = hash_row(seed_h, row0);
    auto h1 = hash_row(h0, row1);

    auto got0 = buf.popHash();
    ASSERT_TRUE(got0.has_value());
    EXPECT_EQ(got0.value(), h0);

    auto got1 = buf.popHash();
    ASSERT_TRUE(got1.has_value());
    EXPECT_EQ(got1.value(), h1);

    EXPECT_EQ(buf.count(), 0U);
}
