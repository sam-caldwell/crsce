/**
 * @file put_le32_writes_little_endian_test.cpp
 * @brief Unit: verify put_le32 writes little-endian bytes at given offset.
 */
#include "common/Format/detail/put_le32.h"
#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <stdexcept>

using crsce::common::format::detail::put_le32;

TEST(Format, PutLe32WritesLittleEndian) { // NOLINT(readability-identifier-naming)
    std::array<std::uint8_t, 28> b{};
    for (auto &x : b) { x = 0xAAU; }
    put_le32(b, 6, 0x01234567U);
    EXPECT_EQ(b[6], 0x67U);
    EXPECT_EQ(b[7], 0x45U);
    EXPECT_EQ(b[8], 0x23U);
    EXPECT_EQ(b[9], 0x01U);
    // Neighbor bytes unchanged
    EXPECT_EQ(b[5], 0xAAU);
    EXPECT_EQ(b[10], 0xAAU);

    // Zero and all-ones
    put_le32(b, 12, 0x00000000U);
    EXPECT_EQ(b[12], 0x00U);
    EXPECT_EQ(b[13], 0x00U);
    EXPECT_EQ(b[14], 0x00U);
    EXPECT_EQ(b[15], 0x00U);
    put_le32(b, 16, 0xFFFFFFFFU);
    EXPECT_EQ(b[16], 0xFFU);
    EXPECT_EQ(b[17], 0xFFU);
    EXPECT_EQ(b[18], 0xFFU);
    EXPECT_EQ(b[19], 0xFFU);
}

TEST(Format, PutLe32OutOfRangeThrows) { // NOLINT(readability-identifier-naming)
    std::array<std::uint8_t, 28> b{};
    // Offset 25 + 3 would index 28 -> out_of_range via array::at
    EXPECT_THROW(put_le32(b, 25, 0xDEADBEEFU), std::out_of_range);
}

TEST(Format, PutLe32WritesAtFinalValidOffset) { // NOLINT(readability-identifier-naming)
    std::array<std::uint8_t, 28> b{};
    for (auto &x : b) { x = 0x00U; }
    // Last valid starting offset for a 4-byte write is 24
    put_le32(b, 24, 0xCAFEBABEU);
    EXPECT_EQ(b[24], 0xBEU);
    EXPECT_EQ(b[25], 0xBAU);
    EXPECT_EQ(b[26], 0xFEU);
    EXPECT_EQ(b[27], 0xCAU);
}
