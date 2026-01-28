/**
 * @file put_le64_writes_little_endian_test.cpp
 * @brief Unit: verify put_le64 writes little-endian bytes at given offset.
 */
#include "common/Format/detail/put_le64.h"
#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <cstddef>

using crsce::common::format::detail::put_le64;

TEST(Format, PutLe64WritesLittleEndian) { // NOLINT(readability-identifier-naming)
    std::array<std::uint8_t, 28> b{};
    for (auto &x : b) { x = 0x77U; }
    const std::uint64_t v = 0x0123456789ABCDEFULL;
    put_le64(b, 8, v);
    EXPECT_EQ(b[8],  0xEFU);
    EXPECT_EQ(b[9],  0xCDU);
    EXPECT_EQ(b[10], 0xABU);
    EXPECT_EQ(b[11], 0x89U);
    EXPECT_EQ(b[12], 0x67U);
    EXPECT_EQ(b[13], 0x45U);
    EXPECT_EQ(b[14], 0x23U);
    EXPECT_EQ(b[15], 0x01U);
    // Neighbor bytes unchanged
    EXPECT_EQ(b[7], 0x77U);
    EXPECT_EQ(b[16], 0x77U);

    // Zero and all-ones
    put_le64(b, 0, 0x0000000000000000ULL);
    EXPECT_EQ(b[0], 0x00U);
    EXPECT_EQ(b[1], 0x00U);
    EXPECT_EQ(b[2], 0x00U);
    EXPECT_EQ(b[3], 0x00U);
    EXPECT_EQ(b[4], 0x00U);
    EXPECT_EQ(b[5], 0x00U);
    EXPECT_EQ(b[6], 0x00U);
    EXPECT_EQ(b[7], 0x00U);
    put_le64(b, 20, 0xFFFFFFFFFFFFFFFFULL);
    EXPECT_EQ(b[20], 0xFFU);
    EXPECT_EQ(b[21], 0xFFU);
    EXPECT_EQ(b[22], 0xFFU);
    EXPECT_EQ(b[23], 0xFFU);
    EXPECT_EQ(b[24], 0xFFU);
    EXPECT_EQ(b[25], 0xFFU);
    EXPECT_EQ(b[26], 0xFFU);
    EXPECT_EQ(b[27], 0xFFU);
}

TEST(Format, PutLe64OutOfRangeThrows) { // NOLINT(readability-identifier-naming)
    std::array<std::uint8_t, 28> b{};
    // Offset 21 + 7 would index 28 -> out_of_range via array::at
    EXPECT_THROW(put_le64(b, 21, 0xDEADBEEFDEADBEEFULL), std::out_of_range);
}
