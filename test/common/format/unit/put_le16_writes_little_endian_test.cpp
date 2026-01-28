/**
 * @file put_le16_writes_little_endian_test.cpp
 * @brief Unit: verify put_le16 writes little-endian bytes at given offset.
 */
#include "common/Format/detail/put_le16.h"
#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <stdexcept>

using crsce::common::format::detail::put_le16;

TEST(Format, PutLe16WritesLittleEndian) { // NOLINT(readability-identifier-naming)
    std::array<std::uint8_t, 28> b{};
    for (auto &x : b) { x = 0xCDU; }
    put_le16(b, 4, 0x1234U);
    EXPECT_EQ(b[4], 0x34U);
    EXPECT_EQ(b[5], 0x12U);
    // Other bytes untouched around the write region
    EXPECT_EQ(b[3], 0xCDU);
    EXPECT_EQ(b[6], 0xCDU);

    // Zero and all-ones
    put_le16(b, 10, 0x0000U);
    EXPECT_EQ(b[10], 0x00U);
    EXPECT_EQ(b[11], 0x00U);
    put_le16(b, 12, 0xFFFFU);
    EXPECT_EQ(b[12], 0xFFU);
    EXPECT_EQ(b[13], 0xFFU);
}

TEST(Format, PutLe16OutOfRangeThrows) { // NOLINT(readability-identifier-naming)
    std::array<std::uint8_t, 28> b{};
    // Writing at offset 27 requires two bytes and should throw via array::at
    EXPECT_THROW(put_le16(b, 27, 0xABCDU), std::out_of_range);
}

TEST(Format, PutLe16WritesAtFinalValidOffset) { // NOLINT(readability-identifier-naming)
    std::array<std::uint8_t, 28> b{};
    for (auto &x : b) { x = 0x00U; }
    // Last valid starting offset for a 2-byte write is 26
    put_le16(b, 26, 0xBEEFU);
    EXPECT_EQ(b[26], 0xEFU);
    EXPECT_EQ(b[27], 0xBEU);
}
