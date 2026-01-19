/**
 * @file sha256_abcdbcde_test.cpp
 * @brief Verify SHA-256 of the multi-block test vector.
 */
#include "common/BitHashBuffer/detail/Sha256.h"
#include <array>
#include <gtest/gtest.h>

using crsce::common::detail::sha256::sha256_digest;
using u8 = crsce::common::detail::sha256::u8;

TEST(Sha256KnownVectors, ABCDBCDEDerived) { // NOLINT(readability-identifier-naming)
    const auto *const msg =
            "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    constexpr std::array<u8, 32> expect{
        0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8,
        0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39,
        0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67,
        0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1
    };
    const auto got = sha256_digest(reinterpret_cast<const u8 *>(msg), 56); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    EXPECT_EQ(got, expect);
}
