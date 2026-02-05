/**
 * @file bithashbuffer_chain_rows_test.cpp
 * @brief BitHashBuffer produces per-row digests (no chaining) and FIFO pop.
 * @copyright (c) 2026 Sam Caldwell.
 * See LICENSE.txt for details.
 */
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>

#include "../../../../include/common/BitHashBuffer/BitHashBuffer.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"

using crsce::common::BitHashBuffer;
using crsce::common::detail::sha256::sha256_digest;

namespace {} // anonymous namespace

/**
 * @name BitHashBufferRowHashTest.TwoRowsAndPopFifo
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(BitHashBufferRowHashTest, TwoRowsAndPopFifo) {
    const std::string seed = "seed"; // simple case, single-block padding
    BitHashBuffer buf(seed);

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

    // Build expected rows and per-row hashes
    std::array<std::uint8_t, BitHashBuffer::kRowSize> row0{};
    row0.fill(0x80);
    std::array<std::uint8_t, BitHashBuffer::kRowSize> row1{};
    row1.fill(0x01);
    auto h0 = sha256_digest(row0.data(), row0.size());
    auto h1 = sha256_digest(row1.data(), row1.size());

    auto got0 = buf.popHash();
    ASSERT_TRUE(got0.has_value());
    EXPECT_EQ(got0.value(), h0);

    auto got1 = buf.popHash();
    ASSERT_TRUE(got1.has_value());
    EXPECT_EQ(got1.value(), h1);

    EXPECT_EQ(buf.count(), 0U);
}
