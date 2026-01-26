/**
 * @file util_crc32_known_vector_test.cpp
 * @brief Verify CRC-32 for the classic "123456789" vector.
 */
#include "common/Util/detail/crc32_ieee.h"
#include <gtest/gtest.h>
#include <array>
#include <cstdint>

using crsce::common::util::crc32_ieee;

/**

 * @name UtilCrc32.KnownVector

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(UtilCrc32, KnownVector) { // NOLINT
  static constexpr std::array<std::uint8_t, 9> v{
      '1','2','3','4','5','6','7','8','9'};
  const auto crc = crc32_ieee(v.data(), v.size());
  EXPECT_EQ(crc, 0xCBF43926U);
}
