/**
 * @file unit_sha1_digest_test.cpp
 * @brief Unit tests for SHA-1 digest computation.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>

#include "common/BitHashBuffer/sha1/sha1_digest.h"

namespace crsce::common::detail::sha1 {

/// @brief Test SHA-1 digest of empty string matches NIST test vector.
TEST(Sha1DigestTest, EmptyString) {
    const auto result = sha1_digest(nullptr, 0);

    const std::array<std::uint8_t, 20> expected{{
        0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55,
        0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09
    }};

    EXPECT_EQ(result, expected);
}

/// @brief Test SHA-1 digest of "abc" matches NIST test vector.
TEST(Sha1DigestTest, AbcString) {
    const std::array<std::uint8_t, 3> input{{'a', 'b', 'c'}};
    const auto result = sha1_digest(input.data(), input.size());

    const std::array<std::uint8_t, 20> expected{{
        0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a, 0xba, 0x3e,
        0x25, 0x71, 0x78, 0x50, 0xc2, 0x6c, 0x9c, 0xd0, 0xd8, 0x9d
    }};

    EXPECT_EQ(result, expected);
}

/// @brief Test SHA-1 digest of 64 zero bytes matches NIST test vector.
TEST(Sha1DigestTest, SixtyFourZeroBytes) {
    std::array<std::uint8_t, 64> input{};
    input.fill(0x00);

    const auto result = sha1_digest(input.data(), input.size());

    const std::array<std::uint8_t, 20> expected{{
        0xc8, 0xd7, 0xd0, 0xef, 0x0e, 0xed, 0xfa, 0x82, 0xd2, 0xea,
        0x1a, 0xa5, 0x92, 0x84, 0x5b, 0x9a, 0x6d, 0x4b, 0x02, 0xb7
    }};

    EXPECT_EQ(result, expected);
}

/// @brief Test SHA-1 digest is deterministic (same input produces same output).
TEST(Sha1DigestTest, Deterministic) {
    const std::array<std::uint8_t, 5> input{{'h', 'e', 'l', 'l', 'o'}};

    const auto result1 = sha1_digest(input.data(), input.size());
    const auto result2 = sha1_digest(input.data(), input.size());

    EXPECT_EQ(result1, result2);
}

/// @brief Test SHA-1 digest differs for different inputs.
TEST(Sha1DigestTest, DifferentInputsDifferentOutputs) {
    const std::array<std::uint8_t, 5> input1{{'h', 'e', 'l', 'l', 'o'}};
    const std::array<std::uint8_t, 5> input2{{'w', 'o', 'r', 'l', 'd'}};

    const auto result1 = sha1_digest(input1.data(), input1.size());
    const auto result2 = sha1_digest(input2.data(), input2.size());

    EXPECT_NE(result1, result2);
}

}  // namespace crsce::common::detail::sha1
