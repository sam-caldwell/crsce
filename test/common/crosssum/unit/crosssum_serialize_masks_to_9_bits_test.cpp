/**
 * @file crosssum_serialize_masks_to_9_bits_test.cpp
 * @brief Ensure CrossSum::serialize_append masks values to 9 bits (0x01FF).
 */
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "common/CrossSum/CrossSum.h"

using crsce::common::CrossSum;

namespace {
    std::vector<std::uint16_t> decode(const std::vector<std::uint8_t> &buf) {
        std::vector<std::uint16_t> vals(CrossSum::kSize, 0);
        for (std::size_t i = 0; i < CrossSum::kSize; ++i) {
            std::uint16_t v = 0;
            for (std::size_t b = 0; b < CrossSum::kBitWidth; ++b) {
                const std::size_t bit_index = (i * CrossSum::kBitWidth) + b;
                const std::size_t byte_index = bit_index / 8;
                const int bit_in_byte = static_cast<int>(bit_index % 8);
                const std::uint8_t byte = buf.at(byte_index);
                const auto bit = static_cast<std::uint16_t>((byte >> (7 - bit_in_byte)) & 0x1U);
                v = static_cast<std::uint16_t>((v << 1) | bit);
            }
            vals[i] = v;
        }
        return vals;
    }
}

/**
 * @name CrossSumSerializeMask.ValuesAboveNineBitsAreMasked
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(CrossSumSerializeMask, ValuesAboveNineBitsAreMasked) { // NOLINT
    CrossSum cs;
    // Manually increment to create values above 9 bits (>= 512)
    // idx 0: set to 1023 -> expect 1023 & 0x1FF == 511
    for (int i = 0; i < 1023; ++i) { cs.increment(0); }
    // idx 1: set to 600 -> expect 600 & 0x1FF == 88
    for (int i = 0; i < 600; ++i) { cs.increment(1); }

    std::vector<std::uint8_t> out;
    cs.serialize_append(out);
    ASSERT_EQ(out.size(), 575U);

    const auto vals = decode(out);
    ASSERT_EQ(vals.size(), CrossSum::kSize);
    EXPECT_EQ(vals[0], 511);
    EXPECT_EQ(vals[1], 88);
}
