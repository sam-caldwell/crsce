/**
 * @file csm_test.cpp
 * @brief Unit tests for CSM (Cross-Sum Matrix).
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "common/Csm/Csm.h"

namespace {

    using crsce::common::Csm;

    // -----------------------------------------------------------------------
    // Default construction: all cells are 0
    // -----------------------------------------------------------------------
    TEST(CsmTest, DefaultConstructionAllZero) {
        const Csm csm;
        // Spot-check several cells across the matrix
        EXPECT_EQ(csm.get(0, 0), 0);
        EXPECT_EQ(csm.get(0, 510), 0);
        EXPECT_EQ(csm.get(510, 0), 0);
        EXPECT_EQ(csm.get(510, 510), 0);
        EXPECT_EQ(csm.get(255, 255), 0);
        EXPECT_EQ(csm.get(100, 400), 0);
    }

    // -----------------------------------------------------------------------
    // set/get: set a cell to 1, verify get returns 1
    // -----------------------------------------------------------------------
    TEST(CsmTest, SetToOneAndGet) {
        Csm csm;
        csm.set(10, 20, 1);
        EXPECT_EQ(csm.get(10, 20), 1);
    }

    // -----------------------------------------------------------------------
    // set/get: set a cell to 0 after setting to 1
    // -----------------------------------------------------------------------
    TEST(CsmTest, SetToOneAndBackToZero) {
        Csm csm;
        csm.set(42, 99, 1);
        EXPECT_EQ(csm.get(42, 99), 1);
        csm.set(42, 99, 0);
        EXPECT_EQ(csm.get(42, 99), 0);
    }

    // -----------------------------------------------------------------------
    // Boundary: cells at (0,0), (510,510), (0,510), (510,0)
    // -----------------------------------------------------------------------
    TEST(CsmTest, BoundaryCorners) {
        Csm csm;

        // (0, 0)
        csm.set(0, 0, 1);
        EXPECT_EQ(csm.get(0, 0), 1);

        // (510, 510)
        csm.set(510, 510, 1);
        EXPECT_EQ(csm.get(510, 510), 1);

        // (0, 510)
        csm.set(0, 510, 1);
        EXPECT_EQ(csm.get(0, 510), 1);

        // (510, 0)
        csm.set(510, 0, 1);
        EXPECT_EQ(csm.get(510, 0), 1);
    }

    // -----------------------------------------------------------------------
    // Setting one cell does not affect adjacent cells
    // -----------------------------------------------------------------------
    TEST(CsmTest, SetDoesNotAffectNeighbors) {
        Csm csm;
        csm.set(100, 100, 1);
        EXPECT_EQ(csm.get(100, 100), 1);
        EXPECT_EQ(csm.get(100, 99), 0);
        EXPECT_EQ(csm.get(100, 101), 0);
        EXPECT_EQ(csm.get(99, 100), 0);
        EXPECT_EQ(csm.get(101, 100), 0);
    }

    // -----------------------------------------------------------------------
    // popcount: empty CSM has popcount 0 for every row
    // -----------------------------------------------------------------------
    TEST(CsmTest, PopcountEmptyMatrix) {
        const Csm csm;
        EXPECT_EQ(csm.popcount(0), 0);
        EXPECT_EQ(csm.popcount(255), 0);
        EXPECT_EQ(csm.popcount(510), 0);
    }

    // -----------------------------------------------------------------------
    // popcount: set a few cells and verify
    // -----------------------------------------------------------------------
    TEST(CsmTest, PopcountAfterSets) {
        Csm csm;
        // Set 3 bits in row 7
        csm.set(7, 0, 1);
        csm.set(7, 63, 1);
        csm.set(7, 510, 1);
        EXPECT_EQ(csm.popcount(7), 3);

        // Row 8 should still be 0
        EXPECT_EQ(csm.popcount(8), 0);

        // Set another bit in row 7
        csm.set(7, 64, 1);
        EXPECT_EQ(csm.popcount(7), 4);
    }

    // -----------------------------------------------------------------------
    // popcount: setting a bit back to 0 reduces popcount
    // -----------------------------------------------------------------------
    TEST(CsmTest, PopcountAfterClearingBit) {
        Csm csm;
        csm.set(3, 10, 1);
        csm.set(3, 20, 1);
        EXPECT_EQ(csm.popcount(3), 2);
        csm.set(3, 10, 0);
        EXPECT_EQ(csm.popcount(3), 1);
    }

    // -----------------------------------------------------------------------
    // vec(): returns row-major vector of bits, verify a few known positions
    // -----------------------------------------------------------------------
    TEST(CsmTest, VecEmptyMatrix) {
        const Csm csm;
        const auto v = csm.vec();
        // Total bits = 511 * 511 = 261121; total bytes = ceil(261121/8) = 32641
        EXPECT_EQ(v.size(), 32641U);
        // All bytes should be 0 for an empty matrix
        for (const auto byte : v) {
            EXPECT_EQ(byte, 0);
        }
    }

    TEST(CsmTest, VecKnownPositions) {
        Csm csm;

        // Set bit (0, 0): this is the first bit in the vec output.
        // Row-major bit 0 => byte 0, bit position 7 (MSB of first byte).
        csm.set(0, 0, 1);
        auto v = csm.vec();
        EXPECT_EQ(v[0] & 0x80U, 0x80U) << "Bit (0,0) should be MSB of byte 0";

        // Set bit (0, 7): row-major bit 7 => byte 0, bit position 0.
        csm.set(0, 7, 1);
        v = csm.vec();
        EXPECT_EQ(v[0] & 0x01U, 0x01U) << "Bit (0,7) should be LSB of byte 0";

        // Set bit (1, 0): row-major bit 511 => byte 511/8 = 63, bit position 7 - (511 % 8) = 7 - 7 = 0.
        csm.set(1, 0, 1);
        v = csm.vec();
        const std::uint32_t bitIdx = 511;
        const std::uint32_t byteIdx = bitIdx / 8; // 63
        const auto bitPos = static_cast<std::uint8_t>(7 - (bitIdx % 8)); // 0
        EXPECT_NE(v[byteIdx] & (1U << bitPos), 0U) << "Bit (1,0) should be set in the vec output";
    }

    // -----------------------------------------------------------------------
    // getRow(): returns array of 8 uint64 words for a row, verify bit positions
    // -----------------------------------------------------------------------
    TEST(CsmTest, GetRowEmpty) {
        const Csm csm;
        const auto row = csm.getRow(0);
        for (std::uint16_t w = 0; w < Csm::kWordsPerRow; ++w) {
            EXPECT_EQ(row.at(w), 0ULL);
        }
    }

    TEST(CsmTest, GetRowBitPositions) {
        Csm csm;

        // Column 0 maps to word 0, bit 63 (MSB).
        csm.set(5, 0, 1);
        auto row = csm.getRow(5);
        EXPECT_NE(row[0] & (1ULL << 63), 0ULL) << "Column 0 should set bit 63 of word 0";

        // Column 63 maps to word 0, bit 0 (LSB).
        csm.set(5, 63, 1);
        row = csm.getRow(5);
        EXPECT_NE(row[0] & 1ULL, 0ULL) << "Column 63 should set bit 0 of word 0";

        // Column 64 maps to word 1, bit 63.
        csm.set(5, 64, 1);
        row = csm.getRow(5);
        EXPECT_NE(row[1] & (1ULL << 63), 0ULL) << "Column 64 should set bit 63 of word 1";

        // Column 510 maps to word 7 (510/64 = 7), bit 63 - (510 % 64) = 63 - 62 = 1.
        csm.set(5, 510, 1);
        row = csm.getRow(5);
        EXPECT_NE(row[7] & (1ULL << 1), 0ULL) << "Column 510 should set bit 1 of word 7";
    }

    TEST(CsmTest, GetRowIsolation) {
        Csm csm;
        csm.set(10, 50, 1);
        // Row 10 should have the bit set
        const auto row10 = csm.getRow(10);
        const auto word = static_cast<std::uint16_t>(50 / 64); // 0
        const auto bit = static_cast<std::uint16_t>(63 - (50 % 64)); // 13
        EXPECT_NE(row10[word] & (1ULL << bit), 0ULL);
        // Row 11 should be all zero
        const auto row11 = csm.getRow(11);
        for (std::uint16_t w = 0; w < Csm::kWordsPerRow; ++w) {
            EXPECT_EQ(row11.at(w), 0ULL);
        }
    }

    // -----------------------------------------------------------------------
    // Multiple bits across word boundaries
    // -----------------------------------------------------------------------
    TEST(CsmTest, MultipleBitsAcrossWords) {
        Csm csm;
        // Set bits at column boundaries: 0, 63, 64, 127, 128, ...
        csm.set(0, 0, 1);
        csm.set(0, 63, 1);
        csm.set(0, 64, 1);
        csm.set(0, 127, 1);
        csm.set(0, 128, 1);
        EXPECT_EQ(csm.popcount(0), 5);

        const auto row = csm.getRow(0);
        // word 0 should have bits 63 and 0 set
        EXPECT_NE(row[0] & (1ULL << 63), 0ULL);
        EXPECT_NE(row[0] & 1ULL, 0ULL);
        // word 1 should have bits 63 and 0 set
        EXPECT_NE(row[1] & (1ULL << 63), 0ULL);
        EXPECT_NE(row[1] & 1ULL, 0ULL);
        // word 2 should have bit 63 set
        EXPECT_NE(row[2] & (1ULL << 63), 0ULL);
    }

} // namespace
