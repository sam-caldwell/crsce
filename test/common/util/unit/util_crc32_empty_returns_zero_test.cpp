/**
 * @file util_crc32_empty_returns_zero_test.cpp
 * @brief CRC-32 empty-case should return zero.
 */
#include "common/Util/detail/crc32_ieee.h"
#include <gtest/gtest.h>
#include <cstdint>

using crsce::common::util::crc32_ieee;

/**

 * @name UtilCrc32.EmptyReturnsZero

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(UtilCrc32, EmptyReturnsZero) { // NOLINT
  const auto crc_null = crc32_ieee(nullptr, 0);
  EXPECT_EQ(crc_null, 0x00000000U);

  // Also verify non-null pointer with zero length is handled equivalently
  const std::uint8_t dummy{0xAA};
  const auto crc_nonnull = crc32_ieee(&dummy, 0);
  EXPECT_EQ(crc_nonnull, 0x00000000U);
}
