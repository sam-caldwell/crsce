/**
 * @file compress_accumulate_row0_small_bits_test.cpp
 * @brief Verify cross-sums and LH count for a small first-row pattern.
 */
#include "compress/Compress/Compress.h"
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <vector>

using crsce::compress::Compress;

namespace {
    // Decode 9-bit MSB-first stream of 511 values from a byte buffer segment
    std::vector<std::uint16_t> decode_9bit_segment(const std::vector<std::uint8_t> &buf,
                                                   std::size_t byte_offset) {
        std::vector<std::uint16_t> vals(Compress::kS, 0);
        for (std::size_t i = 0; i < Compress::kS; ++i) {
            std::uint16_t v = 0;
            for (std::size_t b = 0; b < 9; ++b) {
                const std::size_t bit_index = (i * 9) + b;
                const std::size_t byte_index = byte_offset + (bit_index / 8);
                const int bit_in_byte = static_cast<int>(bit_index % 8);
                const std::uint8_t byte = buf.at(byte_index);
                const auto bit = static_cast<std::uint16_t>((byte >> (7 - bit_in_byte)) & 0x1U);
                v = static_cast<std::uint16_t>((v << 1) | bit);
            }
            vals[i] = v;
        }
        return vals;
    }
} // namespace

/**

 * @name CompressTest.Row0FirstNineBitsSet

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(CompressTest, Row0FirstNineBitsSet) {
    Compress c{"in.bin", "out.bin"};
    // Set the first 9 columns of row 0 to 1; rest zero; finalize block
    for (int i = 0; i < 9; ++i) {
        c.push_bit(true);
    }
    c.finalize_block();

    // LH: one completed row should be hashed
    EXPECT_EQ(c.lh_count(), 1U);

    // Cross-sums raw values
    EXPECT_EQ(c.lsm().value(0), 9); // row 0 has nine ones
    EXPECT_EQ(c.lsm().value(1), 0); // row 1 untouched
    EXPECT_EQ(c.vsm().value(0), 1); // columns 0..8 have ones
    EXPECT_EQ(c.vsm().value(8), 1);
    EXPECT_EQ(c.vsm().value(9), 0);
    EXPECT_EQ(c.dsm().value(0), 1); // r=0 => d=c
    EXPECT_EQ(c.dsm().value(8), 1);
    EXPECT_EQ(c.dsm().value(9), 0);
    EXPECT_EQ(c.xsm().value(0), 1); // x(0,0)=0
    EXPECT_EQ(c.xsm().value(Compress::kS - 1), 1); // x(0,1)=kS-1

    // Serialized cross-sums (LSM,VSM,DSM,XSM)
    const auto bytes = c.serialize_cross_sums();
    ASSERT_EQ(bytes.size(), 4U * 575U);

    // Decode LSM (first 575 bytes)
    const auto lsm = decode_9bit_segment(bytes, 0);
    EXPECT_EQ(lsm.at(0), 9);
    EXPECT_EQ(lsm.at(1), 0);
    EXPECT_EQ(lsm.at(2), 0);

    // Decode VSM (next 575 bytes)
    const auto vsm = decode_9bit_segment(bytes, 575);
    EXPECT_EQ(vsm.at(0), 1);
    EXPECT_EQ(vsm.at(8), 1);
    EXPECT_EQ(vsm.at(9), 0);
}
