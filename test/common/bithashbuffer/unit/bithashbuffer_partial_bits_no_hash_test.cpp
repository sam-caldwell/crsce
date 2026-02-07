/**
 * @file bithashbuffer_partial_bits_no_hash_test.cpp
 * @brief Partial row bits should not produce a hash until padded.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include <gtest/gtest.h>

#include "../../../../include/common/BitHashBuffer/BitHashBuffer.h"

using crsce::common::BitHashBuffer;

/**
 * @name BitHashBufferPartialBitsNoHashTest.PushingLessThanOneRowProducesNoHash
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(BitHashBufferPartialBitsNoHashTest, PushingLessThanOneRowProducesNoHash) {
    BitHashBuffer buf("y");
    // Push 63 bytes worth of zeros (but stop one byte short)
    for (int i = 0; i < 63; ++i) {
        for (int b = 0; b < 8; ++b) {
            buf.pushBit(false);
        }
    }
    EXPECT_EQ(buf.count(), 0U);

    // Push only 7 bits of the next byte; still no hash.
    for (int b = 0; b < 7; ++b) {
        buf.pushBit(false);
    }
    EXPECT_EQ(buf.count(), 0U);
}
