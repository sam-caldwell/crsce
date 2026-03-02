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

    constexpr std::uint16_t kS = 511;

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
        EXPECT_EQ(rs.get(255, 100), 0);
        EXPECT_EQ(rs.get(510, 510), 0);
    }

    TEST(RowSumTest, SetAccumulatesInRow) {
        RowSum rs(kS);
        // Accumulate value 1 at cells (5, 0), (5, 100), (5, 510)
        rs.set(5, 0, 1);
        rs.set(5, 100, 1);
        rs.set(5, 510, 1);
        // get(5, anything) should return 3 (all share row 5)
        EXPECT_EQ(rs.get(5, 0), 3);
        EXPECT_EQ(rs.get(5, 200), 3);
        // Different row should still be 0
        EXPECT_EQ(rs.get(6, 0), 0);
    }

    TEST(RowSumTest, LenAlwaysReturnsS) {
        const RowSum rs(kS);
        EXPECT_EQ(rs.len(0), kS);
        EXPECT_EQ(rs.len(255), kS);
        EXPECT_EQ(rs.len(510), kS);
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
        EXPECT_EQ(cs.get(100, 255), 0);
        EXPECT_EQ(cs.get(510, 510), 0);
    }

    TEST(ColSumTest, SetAccumulatesInColumn) {
        ColSum cs(kS);
        // Accumulate value 1 at cells (0, 7), (100, 7), (510, 7)
        cs.set(0, 7, 1);
        cs.set(100, 7, 1);
        cs.set(510, 7, 1);
        // get(anything, 7) should return 3 (all share column 7)
        EXPECT_EQ(cs.get(0, 7), 3);
        EXPECT_EQ(cs.get(200, 7), 3);
        // Different column should still be 0
        EXPECT_EQ(cs.get(0, 8), 0);
    }

    TEST(ColSumTest, LenAlwaysReturnsS) {
        const ColSum cs(kS);
        EXPECT_EQ(cs.len(0), kS);
        EXPECT_EQ(cs.len(255), kS);
        EXPECT_EQ(cs.len(510), kS);
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

    TEST(DiagSumTest, SizeEquals1021) {
        const DiagSum ds(kS);
        // 2 * 511 - 1 = 1021
        EXPECT_EQ(ds.size(), 1021);
    }

    TEST(DiagSumTest, InitialSumsAreZero) {
        const DiagSum ds(kS);
        EXPECT_EQ(ds.get(0, 0), 0);
        EXPECT_EQ(ds.get(255, 255), 0);
        EXPECT_EQ(ds.get(510, 510), 0);
    }

    TEST(DiagSumTest, LineIndexFormula) {
        // lineIndex(r, c) = c - r + (s - 1)
        DiagSum ds(kS);

        // (0, 0): lineIndex = 0 - 0 + 510 = 510
        ds.set(0, 0, 1);
        EXPECT_EQ(ds.getByIndex(510), 1);

        // (0, 510): lineIndex = 510 - 0 + 510 = 1020
        ds.set(0, 510, 1);
        EXPECT_EQ(ds.getByIndex(1020), 1);

        // (510, 0): lineIndex = 0 - 510 + 510 = 0
        ds.set(510, 0, 1);
        EXPECT_EQ(ds.getByIndex(0), 1);

        // (510, 510): lineIndex = 510 - 510 + 510 = 510
        // This shares the diagonal with (0, 0), so that index should now be 2
        ds.set(510, 510, 1);
        EXPECT_EQ(ds.getByIndex(510), 2);
    }

    TEST(DiagSumTest, SetAccumulatesOnDiagonal) {
        DiagSum ds(kS);
        // Cells (0, 0) and (1, 1) are on the same diagonal (lineIndex = 510)
        ds.set(0, 0, 1);
        ds.set(1, 1, 1);
        EXPECT_EQ(ds.get(0, 0), 2);
        EXPECT_EQ(ds.get(1, 1), 2);
        // Cell (0, 1) is on a different diagonal (lineIndex = 511)
        EXPECT_EQ(ds.get(0, 1), 0);
    }

    TEST(DiagSumTest, LenCornerDiagonals) {
        const DiagSum ds(kS);
        // k = 0: min(1, 511, 1020) = 1
        EXPECT_EQ(ds.len(0), 1);
        // k = 1020 (last diagonal): min(1021, 511, 0) = 0? No: 2*511-1-1020 = 0. min(1021,511,0) = 0?
        // Actually 2*511-1 = 1021, 1021-1-1020 = 0. But a diagonal of length 0 is suspicious.
        // Wait: k ranges [0, 2s-2] = [0, 1020]. k=1020: min(1021, 511, 1021-1-1020) = min(1021,511,0) = 0.
        // Actually that's wrong. Let me recalculate: 2*s - 1 - k = 2*511 - 1 - 1020 = 1022 - 1 - 1020 = 1.
        // So min(1021, 511, 1) = 1.
        EXPECT_EQ(ds.len(1020), 1);
        // k = 510 (main diagonal): min(511, 511, 1021-1-510) = min(511, 511, 510) = 510?
        // 2*511-1-510 = 1022-1-510 = 511. min(511, 511, 511) = 511.
        EXPECT_EQ(ds.len(510), 511);
    }

    TEST(DiagSumTest, LenSymmetric) {
        const DiagSum ds(kS);
        // Diagonal lengths should be symmetric: len(k) == len(2s-2-k)
        EXPECT_EQ(ds.len(0), ds.len(1020));
        EXPECT_EQ(ds.len(1), ds.len(1019));
        EXPECT_EQ(ds.len(100), ds.len(920));
        EXPECT_EQ(ds.len(510), ds.len(510)); // main diagonal is its own mirror
    }

    TEST(DiagSumTest, LenGrowsThenShrinks) {
        const DiagSum ds(kS);
        // len should increase from 1 to 511, then decrease back to 1
        EXPECT_EQ(ds.len(0), 1);
        EXPECT_EQ(ds.len(1), 2);
        EXPECT_EQ(ds.len(2), 3);
        EXPECT_EQ(ds.len(509), 510);
        EXPECT_EQ(ds.len(510), 511);
        EXPECT_EQ(ds.len(511), 510);
        EXPECT_EQ(ds.len(1019), 2);
        EXPECT_EQ(ds.len(1020), 1);
    }

    TEST(DiagSumTest, GetByIndexAndSetByIndex) {
        DiagSum ds(kS);
        ds.setByIndex(510, 99);
        EXPECT_EQ(ds.getByIndex(510), 99);
        EXPECT_EQ(ds.getByIndex(0), 0);
    }

    // =======================================================================
    // AntiDiagSum tests
    // =======================================================================

    TEST(AntiDiagSumTest, SizeEquals1021) {
        const AntiDiagSum xs(kS);
        EXPECT_EQ(xs.size(), 1021);
    }

    TEST(AntiDiagSumTest, InitialSumsAreZero) {
        const AntiDiagSum xs(kS);
        EXPECT_EQ(xs.get(0, 0), 0);
        EXPECT_EQ(xs.get(255, 255), 0);
        EXPECT_EQ(xs.get(510, 510), 0);
    }

    TEST(AntiDiagSumTest, LineIndexFormula) {
        // lineIndex(r, c) = r + c
        AntiDiagSum xs(kS);

        // (0, 0): lineIndex = 0
        xs.set(0, 0, 1);
        EXPECT_EQ(xs.getByIndex(0), 1);

        // (510, 510): lineIndex = 1020
        xs.set(510, 510, 1);
        EXPECT_EQ(xs.getByIndex(1020), 1);

        // (0, 510): lineIndex = 510
        xs.set(0, 510, 1);
        EXPECT_EQ(xs.getByIndex(510), 1);

        // (510, 0): lineIndex = 510 (same anti-diagonal as (0, 510))
        xs.set(510, 0, 1);
        EXPECT_EQ(xs.getByIndex(510), 2);
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
        // k = 0: min(1, 511, 1020) = 1
        EXPECT_EQ(xs.len(0), 1);
        // k = 1020: min(1021, 511, 2*511-1-1020) = min(1021, 511, 1) = 1
        EXPECT_EQ(xs.len(1020), 1);
        // k = 510: min(511, 511, 511) = 511
        EXPECT_EQ(xs.len(510), 511);
    }

    TEST(AntiDiagSumTest, LenSymmetric) {
        const AntiDiagSum xs(kS);
        EXPECT_EQ(xs.len(0), xs.len(1020));
        EXPECT_EQ(xs.len(1), xs.len(1019));
        EXPECT_EQ(xs.len(100), xs.len(920));
    }

    TEST(AntiDiagSumTest, LenGrowsThenShrinks) {
        const AntiDiagSum xs(kS);
        EXPECT_EQ(xs.len(0), 1);
        EXPECT_EQ(xs.len(1), 2);
        EXPECT_EQ(xs.len(509), 510);
        EXPECT_EQ(xs.len(510), 511);
        EXPECT_EQ(xs.len(511), 510);
        EXPECT_EQ(xs.len(1019), 2);
        EXPECT_EQ(xs.len(1020), 1);
    }

    TEST(AntiDiagSumTest, GetByIndexAndSetByIndex) {
        AntiDiagSum xs(kS);
        xs.setByIndex(510, 77);
        EXPECT_EQ(xs.getByIndex(510), 77);
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

        const std::uint16_t r = 100;
        const std::uint16_t c = 200;

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
