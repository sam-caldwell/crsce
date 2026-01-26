/**
 * @file compress_finalize_without_bits_has_no_lh_test.cpp
 * @brief Verify finalize on an empty block does not enqueue LH.
 */
#include "compress/Compress/Compress.h"
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <vector>

using crsce::compress::Compress;

namespace {
    std::vector<std::uint16_t> decode_9bit_vector(const std::vector<std::uint8_t> &buf,
                                                  std::size_t byte_offset) {
        std::vector<std::uint16_t> vals(Compress::kS, 0);
        for (std::size_t i = 0; i < Compress::kS; ++i) {
            std::uint16_t v = 0;
            for (std::size_t b = 0; b < 9; ++b) {
                const std::size_t bit_index = (i * 9) + b;
                const std::size_t byte_index = byte_offset + (bit_index / 8);
                const auto bit_in_byte = static_cast<int>(bit_index % 8);
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
 * @name CompressTest.FinalizeWithoutBitsKeepsZeroVectors
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(CompressTest, FinalizeWithoutBitsKeepsZeroVectors) {
    Compress c{"in.bin", "out.bin"};
    c.finalize_block();
    EXPECT_EQ(c.lh_count(), 0U);
    const auto bytes = c.serialize_cross_sums();
    ASSERT_EQ(bytes.size(), 4U * 575U);
    const auto lsm = decode_9bit_vector(bytes, 0);
    const auto vsm = decode_9bit_vector(bytes, 575);
    const auto dsm = decode_9bit_vector(bytes, 1150);
    const auto xsm = decode_9bit_vector(bytes, 1725);
    for (std::size_t i = 0; i < Compress::kS; ++i) {
        EXPECT_EQ(lsm[i], 0);
        EXPECT_EQ(vsm[i], 0);
        EXPECT_EQ(dsm[i], 0);
        EXPECT_EQ(xsm[i], 0);
    }
}
