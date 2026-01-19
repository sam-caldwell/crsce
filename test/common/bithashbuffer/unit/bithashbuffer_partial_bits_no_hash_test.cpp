/*
 * @file bithashbuffer_partial_bits_no_hash_test.cpp
 * @brief Pushing fewer than 64 bytes worth of bits should not yield a hash.
 */
#include <gtest/gtest.h>
#include <string>

#include "../../../../include/common/BitHashBuffer/BitHashBuffer.h"

using crsce::common::BitHashBuffer;

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
