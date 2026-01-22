/**
 * @file util_crc32_streaming_matches_whole_test.cpp
 * @brief CRC-32 streaming continuation should match whole-buffer result.
 */
#include "common/Util/Crc32.h"
#include <gtest/gtest.h>
#include <array>
#include <cstdint>

using crsce::common::util::crc32_ieee;

/**

 * @name UtilCrc32.StreamingMatchesWhole

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(UtilCrc32, StreamingMatchesWhole) { // NOLINT
  static constexpr std::array<std::uint8_t, 9> whole{
      '1','2','3','4','5','6','7','8','9'};
  const auto whole_crc = crc32_ieee(whole.data(), whole.size());

  static constexpr std::array<std::uint8_t, 3> part1{'1','2','3'};
  static constexpr std::array<std::uint8_t, 6> part2{'4','5','6','7','8','9'};
  const auto crc1 = crc32_ieee(part1.data(), part1.size());
  const auto cont_seed = ~crc1; // continue from previous (invert to reenter internal state)
  const auto crc2 = crc32_ieee(part2.data(), part2.size(), cont_seed);
  EXPECT_EQ(crc2, whole_crc);
}
