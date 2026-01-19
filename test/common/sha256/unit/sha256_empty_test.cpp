/**
 * @file sha256_empty_test.cpp
 * @brief Verify SHA-256("") matches the FIPS 180-4 known vector.
 */
#include "common/BitHashBuffer/detail/Sha256.h"
#include <array>
#include <gtest/gtest.h>

using crsce::common::detail::sha256::sha256_digest;
using u8 = crsce::common::detail::sha256::u8;

TEST(Sha256KnownVectors, EmptyString) {
  const std::array<u8, 32> expect{
      0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
      0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
      0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
      0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55};

  const auto got = sha256_digest(nullptr, 0);
  EXPECT_EQ(got, expect);
}
