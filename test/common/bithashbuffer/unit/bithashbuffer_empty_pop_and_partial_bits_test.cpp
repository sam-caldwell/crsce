/*
 * @file bithashbuffer_empty_pop_and_partial_bits_test.cpp
 * @brief popHash() empty case; partial bits should not produce a hash.
 */
#include <gtest/gtest.h>
#include <string>

#include "../../../../include/common/BitHashBuffer/BitHashBuffer.h"

using crsce::common::BitHashBuffer;

TEST(BitHashBufferEmptyPopPartialBitsTest, EmptyPopReturnsNullopt) {
  BitHashBuffer buf("x");
  EXPECT_EQ(buf.count(), 0U);
  const auto h = buf.popHash();
  EXPECT_FALSE(h.has_value());
}
