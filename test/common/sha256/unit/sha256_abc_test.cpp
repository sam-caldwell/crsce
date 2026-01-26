/**
 * @file sha256_abc_test.cpp
 * @brief Verify SHA-256("abc") matches the known vector.
 */
#include "common/BitHashBuffer/detail/Sha256Types.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"
#include <array>
#include <gtest/gtest.h>

using crsce::common::detail::sha256::sha256_digest;
using u8 = crsce::common::detail::sha256::u8;

/**

 * @name Sha256KnownVectors.ABC

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(Sha256KnownVectors, ABC) {
    const auto *const msg = "abc";
    const std::array<u8, 32> expect{
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    const auto got = sha256_digest(reinterpret_cast<const u8 *>(msg), 3); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    EXPECT_EQ(got, expect);
}
