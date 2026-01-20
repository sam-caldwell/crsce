/**
 * @file crosssum_serialize_roundtrip_test.cpp
 * @brief Test CrossSum 9-bit MSB-first serialization using a decode roundtrip.
 */
#include "common/CrossSum/CrossSum.h"
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <vector>

using crsce::common::CrossSum;

namespace {
    // Decode helper: interpret a 9-bit MSB-first stream into 511 values
    std::vector<std::uint16_t> decode_9bit_stream(const std::vector<std::uint8_t> &buf) {
        std::vector<std::uint16_t> vals(CrossSum::kSize, 0);
        for (std::size_t i = 0; i < CrossSum::kSize; ++i) {
            std::uint16_t v = 0;
            for (std::size_t b = 0; b < CrossSum::kBitWidth; ++b) {
                const std::size_t bit_index = (i * CrossSum::kBitWidth) + b;
                const std::size_t byte_index = bit_index / 8;
                const auto bit_in_byte = static_cast<int>(bit_index % 8);
                const std::uint8_t byte = buf.at(byte_index);
                // MSB-first within each byte
                const auto bit = static_cast<std::uint16_t>((byte >> (7 - bit_in_byte)) & 0x1U);
                v = static_cast<std::uint16_t>((v << 1) | bit);
            }
            vals[i] = v;
        }
        return vals;
    }
} // namespace

TEST(CrossSumTest, SerializeAppendProduces575BytesAndRoundtrips) {
    CrossSum cs;
    // Set a selection of indices to small values via increments
    auto set_to = [&](const std::size_t idx, std::uint16_t val) {
        for (std::uint16_t i = 0; i < val; ++i) {
            cs.increment(idx);
        }
    };
    set_to(0, 1);
    set_to(1, 2);
    set_to(2, 3);
    set_to(7, 7);
    set_to(8, 8);
    set_to(9, 9);
    set_to(510, 5);

    std::vector<std::uint8_t> out;
    cs.serialize_append(out);

    // 511 * 9 bits = 4599 bits = 575 bytes
    ASSERT_EQ(out.size(), 575);

    const auto vals = decode_9bit_stream(out);
    ASSERT_EQ(vals.size(), CrossSum::kSize);

    // Expected values
    EXPECT_EQ(vals[0], 1);
    EXPECT_EQ(vals[1], 2);
    EXPECT_EQ(vals[2], 3);
    EXPECT_EQ(vals[7], 7);
    EXPECT_EQ(vals[8], 8);
    EXPECT_EQ(vals[9], 9);
    EXPECT_EQ(vals[510], 5);

    // A few random untouched positions should be zero
    EXPECT_EQ(vals[3], 0);
    EXPECT_EQ(vals[100], 0);
    EXPECT_EQ(vals[400], 0);
}
