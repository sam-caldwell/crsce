/**
 * @file bithashbuffer_empty_pop_and_partial_bits_test.cpp
 * @brief Empty pop and partial bits behavior (no spurious hashes).
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include <gtest/gtest.h>

#include "../../../../include/common/BitHashBuffer/BitHashBuffer.h"

using crsce::common::BitHashBuffer;

/**
 * @name BitHashBufferEmptyPopPartialBitsTest.EmptyPopReturnsNullopt
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(BitHashBufferEmptyPopPartialBitsTest, EmptyPopReturnsNullopt) {
    BitHashBuffer buf("x");
    EXPECT_EQ(buf.count(), 0U);
    const auto h = buf.popHash();
    EXPECT_FALSE(h.has_value());
}
