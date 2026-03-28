/**
 * @file cross_sum_vector_test.cpp
 * @brief Unit tests for CrossSumVector hierarchy (RowSum, ColSum, DiagSum, AntiDiagSum).
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include <gtest/gtest.h>

#include <cstdint>

#include "common/CrossSum/AntiDiagSum.h"
#include "common/CrossSum/ColSum.h"
#include "common/CrossSum/DiagSum.h"
#include "common/CrossSum/RowSum.h"

namespace {

    using crsce::common::AntiDiagSum;
    using crsce::common::ColSum;
    using crsce::common::DiagSum;
    using crsce::common::RowSum;

    constexpr std::uint16_t kS = 127;

    // =======================================================================
    // RowSum tests
    // =======================================================================

    TEST(RowSumTest, SizeEquals511) {
        const RowSum rs(kS);
        EXPECT_EQ(rs.size(), kS);
    }

    TEST(RowSumTest, InitialSumsAreZero) {
        const RowSum rs(kS);
        EXPECT_EQ(rs.get(0, 0), 0);
        EXPECT_EQ(rs.get(63, 50), 0);
        EXPECT_EQ(rs.get(126, 126), 0);
    }

    TEST(RowSumTest, SetAccumulatesInRow) {
        RowSum rs(kS);
        // Accumulate value 1 at cells (5, 0), (5, 50), (5, 126)
        rs.set(5, 0, 1);
        rs.set(5, 50, 1);
        rs.set(5, 126, 1);
        // get(5, anything) should return 3 (all share row 5)
        EXPECT_EQ(rs.get(5, 0), 3);
        EXPECT_EQ(rs.get(5, 80), 3);
        // Different row should still be 0
        EXPECT_EQ(rs.get(6, 0), 0);
    }

    TEST(RowSumTest, LenAlwaysReturnsS) {
        const RowSum rs(kS);
        EXPECT_EQ(rs.len(0), kS);
        EXPECT_EQ(rs.len(63), kS);
        EXPECT_EQ(rs.len(126), kS);
    }

    TEST(RowSumTest, GetByIndexAndSetByIndex) {
        RowSum rs(kS);
        rs.setByIndex(42, 123);
        EXPECT_EQ(rs.getByIndex(42), 123);
        // Other indices remain 0
        EXPECT_EQ(rs.getByIndex(0), 0);
        EXPECT_EQ(rs.getByIndex(43), 0);
    }

    // =======================================================================
    // ColSum tests
    // =======================================================================

    TEST(ColSumTest, SizeEquals511) {
        const ColSum cs(kS);
        EXPECT_EQ(cs.size(), kS);
    }

    TEST(ColSumTest, InitialSumsAreZero) {
        const ColSum cs(kS);
        EXPECT_EQ(cs.get(0, 0), 0);
        EXPECT_EQ(cs.get(50, 63), 0);
        EXPECT_EQ(cs.get(126, 126), 0);
    }

    TEST(ColSumTest, SetAccumulatesInColumn) {
        ColSum cs(kS);
        // Accumulate value 1 at cells (0, 7), (50, 7), (126, 7)
        cs.set(0, 7, 1);
        cs.set(50, 7, 1);
        cs.set(126, 7, 1);
        // get(anything, 7) should return 3 (all share column 7)
        EXPECT_EQ(cs.get(0, 7), 3);
        EXPECT_EQ(cs.get(80, 7), 3);
        // Different column should still be 0
        EXPECT_EQ(cs.get(0, 8), 0);
    }

    TEST(ColSumTest, LenAlwaysReturnsS) {
        const ColSum cs(kS);
        EXPECT_EQ(cs.len(0), kS);
        EXPECT_EQ(cs.len(63), kS);
        EXPECT_EQ(cs.len(126), kS);
    }

    TEST(ColSumTest, GetByIndexAndSetByIndex) {
        ColSum cs(kS);
        cs.setByIndex(100, 456);
        EXPECT_EQ(cs.getByIndex(100), 456);
        EXPECT_EQ(cs.getByIndex(99), 0);
    }

    // =======================================================================
    // DiagSum tests
    // =======================================================================

    TEST(DiagSumTest, SizeEquals253) {
        const DiagSum ds(kS);
        // 2 * 127 - 1 = 253
        EXPECT_EQ(ds.size(), 253);
    }

    TEST(DiagSumTest, InitialSumsAreZero) {
        const DiagSum ds(kS);
        EXPECT_EQ(ds.get(0, 0), 0);
        EXPECT_EQ(ds.get(63, 63), 0);
        EXPECT_EQ(ds.get(126, 126), 0);
    }

    TEST(DiagSumTest, LineIndexFormula) {
        // lineIndex(r, c) = c - r + (s - 1)
        DiagSum ds(kS);

        // (0, 0): lineIndex = 0 - 0 + 126 = 126
        ds.set(0, 0, 1);
        EXPECT_EQ(ds.getByIndex(126), 1);

        // (0, 126): lineIndex = 126 - 0 + 126 = 252
        ds.set(0, 126, 1);
        EXPECT_EQ(ds.getByIndex(252), 1);

        // (126, 0): lineIndex = 0 - 126 + 126 = 0
        ds.set(126, 0, 1);
        EXPECT_EQ(ds.getByIndex(0), 1);

        // (126, 126): lineIndex = 126 - 126 + 126 = 126
        // This shares the diagonal with (0, 0), so that index should now be 2
        ds.set(126, 126, 1);
        EXPECT_EQ(ds.getByIndex(126), 2);
    }

    TEST(DiagSumTest, SetAccumulatesOnDiagonal) {
        DiagSum ds(kS);
        // Cells (0, 0) and (1, 1) are on the same diagonal (lineIndex = 126)
        ds.set(0, 0, 1);
        ds.set(1, 1, 1);
        EXPECT_EQ(ds.get(0, 0), 2);
        EXPECT_EQ(ds.get(1, 1), 2);
        // Cell (0, 1) is on a different diagonal (lineIndex = 127)
        EXPECT_EQ(ds.get(0, 1), 0);
    }

    TEST(DiagSumTest, LenCornerDiagonals) {
        const DiagSum ds(kS);
        // k = 0: min(1, 127, 252) = 1
        EXPECT_EQ(ds.len(0), 1);
        // k = 252 (last diagonal): 2*127 - 1 - 252 = 254 - 1 - 252 = 1. min(253, 127, 1) = 1.
        EXPECT_EQ(ds.len(252), 1);
        // k = 126 (main diagonal): 2*127 - 1 - 126 = 127. min(127, 127, 127) = 127.
        EXPECT_EQ(ds.len(126), 127);
    }

    TEST(DiagSumTest, LenSymmetric) {
        const DiagSum ds(kS);
        // Diagonal lengths should be symmetric: len(k) == len(2s-2-k)
        EXPECT_EQ(ds.len(0), ds.len(252));
        EXPECT_EQ(ds.len(1), ds.len(251));
        EXPECT_EQ(ds.len(50), ds.len(202));
        EXPECT_EQ(ds.len(126), ds.len(126)); // main diagonal is its own mirror
    }

    TEST(DiagSumTest, LenGrowsThenShrinks) {
        const DiagSum ds(kS);
        // len should increase from 1 to 127, then decrease back to 1
        EXPECT_EQ(ds.len(0), 1);
        EXPECT_EQ(ds.len(1), 2);
        EXPECT_EQ(ds.len(2), 3);
        EXPECT_EQ(ds.len(125), 126);
        EXPECT_EQ(ds.len(126), 127);
        EXPECT_EQ(ds.len(127), 126);
        EXPECT_EQ(ds.len(251), 2);
        EXPECT_EQ(ds.len(252), 1);
    }

    TEST(DiagSumTest, GetByIndexAndSetByIndex) {
        DiagSum ds(kS);
        ds.setByIndex(126, 99);
        EXPECT_EQ(ds.getByIndex(126), 99);
        EXPECT_EQ(ds.getByIndex(0), 0);
    }

    // =======================================================================
    // AntiDiagSum tests
    // =======================================================================

    TEST(AntiDiagSumTest, SizeEquals253) {
        const AntiDiagSum xs(kS);
        EXPECT_EQ(xs.size(), 253);
    }

    TEST(AntiDiagSumTest, InitialSumsAreZero) {
        const AntiDiagSum xs(kS);
        EXPECT_EQ(xs.get(0, 0), 0);
        EXPECT_EQ(xs.get(63, 63), 0);
        EXPECT_EQ(xs.get(126, 126), 0);
    }

    TEST(AntiDiagSumTest, LineIndexFormula) {
        // lineIndex(r, c) = r + c
        AntiDiagSum xs(kS);

        // (0, 0): lineIndex = 0
        xs.set(0, 0, 1);
        EXPECT_EQ(xs.getByIndex(0), 1);

        // (126, 126): lineIndex = 252
        xs.set(126, 126, 1);
        EXPECT_EQ(xs.getByIndex(252), 1);

        // (0, 126): lineIndex = 126
        xs.set(0, 126, 1);
        EXPECT_EQ(xs.getByIndex(126), 1);

        // (126, 0): lineIndex = 126 (same anti-diagonal as (0, 126))
        xs.set(126, 0, 1);
        EXPECT_EQ(xs.getByIndex(126), 2);
    }

    TEST(AntiDiagSumTest, SetAccumulatesOnAntiDiagonal) {
        AntiDiagSum xs(kS);
        // Cells (0, 5) and (5, 0) are on the same anti-diagonal (lineIndex = 5)
        xs.set(0, 5, 1);
        xs.set(5, 0, 1);
        EXPECT_EQ(xs.get(0, 5), 2);
        EXPECT_EQ(xs.get(5, 0), 2);
        // Cell (1, 1) is on anti-diagonal 2, not 5
        EXPECT_EQ(xs.get(1, 1), 0);
    }

    TEST(AntiDiagSumTest, LenCornerAntiDiagonals) {
        const AntiDiagSum xs(kS);
        // k = 0: min(1, 127, 252) = 1
        EXPECT_EQ(xs.len(0), 1);
        // k = 252: min(253, 127, 2*127-1-252) = min(253, 127, 1) = 1
        EXPECT_EQ(xs.len(252), 1);
        // k = 126: min(127, 127, 127) = 127
        EXPECT_EQ(xs.len(126), 127);
    }

    TEST(AntiDiagSumTest, LenSymmetric) {
        const AntiDiagSum xs(kS);
        EXPECT_EQ(xs.len(0), xs.len(252));
        EXPECT_EQ(xs.len(1), xs.len(251));
        EXPECT_EQ(xs.len(50), xs.len(202));
    }

    TEST(AntiDiagSumTest, LenGrowsThenShrinks) {
        const AntiDiagSum xs(kS);
        EXPECT_EQ(xs.len(0), 1);
        EXPECT_EQ(xs.len(1), 2);
        EXPECT_EQ(xs.len(125), 126);
        EXPECT_EQ(xs.len(126), 127);
        EXPECT_EQ(xs.len(127), 126);
        EXPECT_EQ(xs.len(251), 2);
        EXPECT_EQ(xs.len(252), 1);
    }

    TEST(AntiDiagSumTest, GetByIndexAndSetByIndex) {
        AntiDiagSum xs(kS);
        xs.setByIndex(126, 77);
        EXPECT_EQ(xs.getByIndex(126), 77);
        EXPECT_EQ(xs.getByIndex(0), 0);
    }

    // =======================================================================
    // Cross-type consistency: accumulating on the same cell across all four
    // =======================================================================
    TEST(CrossSumVectorTest, AllFourTypesAccumulateIndependently) {
        RowSum rs(kS);
        ColSum cs(kS);
        DiagSum ds(kS);
        AntiDiagSum xs(kS);

        const std::uint16_t r = 50;
        const std::uint16_t c = 100;

        rs.set(r, c, 1);
        cs.set(r, c, 1);
        ds.set(r, c, 1);
        xs.set(r, c, 1);

        // Each should have accumulated exactly 1 on the line through (r, c)
        EXPECT_EQ(rs.get(r, c), 1);
        EXPECT_EQ(cs.get(r, c), 1);
        EXPECT_EQ(ds.get(r, c), 1);
        EXPECT_EQ(xs.get(r, c), 1);

        // Adding another cell in the same row (but different column) only affects RowSum
        rs.set(r, c + 1, 1);
        cs.set(r, c + 1, 1);
        ds.set(r, c + 1, 1);
        xs.set(r, c + 1, 1);

        EXPECT_EQ(rs.get(r, c), 2);     // same row
        EXPECT_EQ(cs.get(r, c), 1);     // different column
        EXPECT_EQ(ds.get(r, c), 1);     // different diagonal
        EXPECT_EQ(xs.get(r, c), 1);     // different anti-diagonal
    }

    // =======================================================================
    // Small matrix size (smoke test with s=3)
    // =======================================================================
    TEST(CrossSumVectorTest, SmallMatrixRowSum) {
        RowSum rs(3);
        EXPECT_EQ(rs.size(), 3);
        EXPECT_EQ(rs.len(0), 3);
        rs.set(0, 0, 1);
        rs.set(0, 1, 1);
        rs.set(0, 2, 1);
        EXPECT_EQ(rs.get(0, 0), 3);
    }

    TEST(CrossSumVectorTest, SmallMatrixDiagSum) {
        const DiagSum ds(3);
        // size = 2*3 - 1 = 5
        EXPECT_EQ(ds.size(), 5);
        // k=0: len=1, k=1: len=2, k=2: len=3, k=3: len=2, k=4: len=1
        EXPECT_EQ(ds.len(0), 1);
        EXPECT_EQ(ds.len(1), 2);
        EXPECT_EQ(ds.len(2), 3);
        EXPECT_EQ(ds.len(3), 2);
        EXPECT_EQ(ds.len(4), 1);
    }

    TEST(CrossSumVectorTest, SmallMatrixAntiDiagSum) {
        AntiDiagSum xs(3);
        EXPECT_EQ(xs.size(), 5);
        // lineIndex(0,0) = 0, lineIndex(0,1) = 1, lineIndex(1,0) = 1
        xs.set(0, 1, 1);
        xs.set(1, 0, 1);
        EXPECT_EQ(xs.get(0, 1), 2);
        EXPECT_EQ(xs.get(1, 0), 2);
    }

} // namespace
