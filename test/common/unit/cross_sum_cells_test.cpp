/**
 * @file cross_sum_cells_test.cpp
 * @brief Unit tests for the cells() method of RowSum, ColSum, DiagSum, and AntiDiagSum.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "common/CrossSum/AntiDiagSum.h"
#include "common/CrossSum/ColSum.h"
#include "common/CrossSum/DiagSum.h"
#include "common/CrossSum/RowSum.h"

namespace {

    using crsce::common::AntiDiagSum;
    using crsce::common::ColSum;
    using crsce::common::DiagSum;
    using crsce::common::RowSum;

    using CellVec = std::vector<std::pair<std::uint16_t, std::uint16_t>>;

    constexpr std::uint16_t kS = 127;

    // =======================================================================
    // RowSum::cells() tests
    // =======================================================================

    TEST(RowSumCellsTest, CellsReturnsAllColumnsInRow) {
        const RowSum rs(kS);
        const CellVec result = rs.cells(0, 0);
        EXPECT_EQ(result.size(), kS);
        for (std::uint16_t col = 0; col < kS; ++col) {
            EXPECT_EQ(result[col].first, 0);
            EXPECT_EQ(result[col].second, col);
        }
    }

    TEST(RowSumCellsTest, CellsLengthMatchesLen) {
        const RowSum rs(kS);
        for (const std::uint16_t row : {0, 1, 63, 125, 126}) {
            const CellVec result = rs.cells(row, 0);
            EXPECT_EQ(result.size(), rs.len(row));
        }
    }

    TEST(RowSumCellsTest, CellsIgnoresColumnArgument) {
        const RowSum rs(kS);
        // Calling cells() with different c values for the same row should produce identical results.
        const CellVec a = rs.cells(42, 0);
        const CellVec b = rs.cells(42, 50);
        const CellVec c = rs.cells(42, 126);
        EXPECT_EQ(a, b);
        EXPECT_EQ(b, c);
    }

    TEST(RowSumCellsTest, CellsMiddleRow) {
        const RowSum rs(kS);
        const CellVec result = rs.cells(63, 50);
        ASSERT_EQ(result.size(), kS);
        for (std::uint16_t col = 0; col < kS; ++col) {
            EXPECT_EQ(result[col].first, 63);
            EXPECT_EQ(result[col].second, col);
        }
    }

    TEST(RowSumCellsTest, CellsLastRow) {
        const RowSum rs(kS);
        const CellVec result = rs.cells(126, 126);
        ASSERT_EQ(result.size(), kS);
        EXPECT_EQ(result.front(), std::make_pair(static_cast<std::uint16_t>(126), static_cast<std::uint16_t>(0)));
        EXPECT_EQ(result.back(), std::make_pair(static_cast<std::uint16_t>(126), static_cast<std::uint16_t>(126)));
    }

    TEST(RowSumCellsTest, SmallMatrixCells) {
        const RowSum rs(3);
        const CellVec result = rs.cells(1, 2);
        const CellVec expected = {{1, 0}, {1, 1}, {1, 2}};
        EXPECT_EQ(result, expected);
    }

    // =======================================================================
    // ColSum::cells() tests
    // =======================================================================

    TEST(ColSumCellsTest, CellsReturnsAllRowsInColumn) {
        const ColSum cs(kS);
        const CellVec result = cs.cells(0, 0);
        EXPECT_EQ(result.size(), kS);
        for (std::uint16_t row = 0; row < kS; ++row) {
            EXPECT_EQ(result[row].first, row);
            EXPECT_EQ(result[row].second, 0);
        }
    }

    TEST(ColSumCellsTest, CellsLengthMatchesLen) {
        const ColSum cs(kS);
        for (const std::uint16_t col : {0, 1, 63, 125, 126}) {
            const CellVec result = cs.cells(0, col);
            EXPECT_EQ(result.size(), cs.len(col));
        }
    }

    TEST(ColSumCellsTest, CellsIgnoresRowArgument) {
        const ColSum cs(kS);
        // Calling cells() with different r values for the same column should produce identical results.
        const CellVec a = cs.cells(0, 42);
        const CellVec b = cs.cells(50, 42);
        const CellVec c = cs.cells(126, 42);
        EXPECT_EQ(a, b);
        EXPECT_EQ(b, c);
    }

    TEST(ColSumCellsTest, CellsMiddleColumn) {
        const ColSum cs(kS);
        const CellVec result = cs.cells(50, 63);
        ASSERT_EQ(result.size(), kS);
        for (std::uint16_t row = 0; row < kS; ++row) {
            EXPECT_EQ(result[row].first, row);
            EXPECT_EQ(result[row].second, 63);
        }
    }

    TEST(ColSumCellsTest, CellsLastColumn) {
        const ColSum cs(kS);
        const CellVec result = cs.cells(0, 126);
        ASSERT_EQ(result.size(), kS);
        EXPECT_EQ(result.front(), std::make_pair(static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(126)));
        EXPECT_EQ(result.back(), std::make_pair(static_cast<std::uint16_t>(126), static_cast<std::uint16_t>(126)));
    }

    TEST(ColSumCellsTest, SmallMatrixCells) {
        const ColSum cs(3);
        const CellVec result = cs.cells(2, 1);
        const CellVec expected = {{0, 1}, {1, 1}, {2, 1}};
        EXPECT_EQ(result, expected);
    }

    // =======================================================================
    // DiagSum::cells() tests
    // =======================================================================

    TEST(DiagSumCellsTest, CellsIndex0SingleCell) {
        // Diagonal index 0 corresponds to the bottom-left corner cell (126, 0).
        // lineIndex(126, 0) = 0 - 126 + 126 = 0. Length = 1.
        const DiagSum ds(kS);
        const CellVec result = ds.cells(126, 0);
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result[0], std::make_pair(static_cast<std::uint16_t>(126), static_cast<std::uint16_t>(0)));
    }

    TEST(DiagSumCellsTest, CellsIndex252SingleCell) {
        // Diagonal index 252 corresponds to the top-right corner cell (0, 126).
        // lineIndex(0, 126) = 126 - 0 + 126 = 252. Length = 1.
        const DiagSum ds(kS);
        const CellVec result = ds.cells(0, 126);
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result[0], std::make_pair(static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(126)));
    }

    TEST(DiagSumCellsTest, CellsMainDiagonalIndex126) {
        // Diagonal index 126 is the main diagonal. lineIndex(0,0) = 0 - 0 + 126 = 126.
        // Length = 127, cells from (0,0) to (126,126).
        const DiagSum ds(kS);
        const CellVec result = ds.cells(0, 0);
        ASSERT_EQ(result.size(), 127);
        for (std::uint16_t i = 0; i < 127; ++i) {
            EXPECT_EQ(result[i].first, i);
            EXPECT_EQ(result[i].second, i);
        }
    }

    TEST(DiagSumCellsTest, CellsLengthMatchesLen) {
        const DiagSum ds(kS);
        // For a few representative cells, verify cells().size() == len(lineIndex(r,c)).
        // Diagonal index 0: cell (126, 0)
        EXPECT_EQ(ds.cells(126, 0).size(), ds.len(0));
        // Diagonal index 1: cell (126, 1) or (125, 0)
        EXPECT_EQ(ds.cells(126, 1).size(), ds.len(1));
        // Diagonal index 126 (main diagonal)
        EXPECT_EQ(ds.cells(0, 0).size(), ds.len(126));
        // Diagonal index 252: cell (0, 126)
        EXPECT_EQ(ds.cells(0, 126).size(), ds.len(252));
    }

    TEST(DiagSumCellsTest, CellsIndex1TwoCells) {
        // Diagonal index 1: lineIndex(r,c) = c - r + 126 = 1, so c = r - 125.
        // Valid cells: r=125 -> c=0, r=126 -> c=1. Length = 2.
        const DiagSum ds(kS);
        const CellVec result = ds.cells(125, 0);
        ASSERT_EQ(result.size(), 2);
        EXPECT_EQ(result[0], std::make_pair(static_cast<std::uint16_t>(125), static_cast<std::uint16_t>(0)));
        EXPECT_EQ(result[1], std::make_pair(static_cast<std::uint16_t>(126), static_cast<std::uint16_t>(1)));
    }

    TEST(DiagSumCellsTest, CellsIndex251TwoCells) {
        // Diagonal index 251: lineIndex(r,c) = c - r + 126 = 251, so c = r + 125.
        // Valid cells: r=0 -> c=125, r=1 -> c=126. Length = 2.
        const DiagSum ds(kS);
        const CellVec result = ds.cells(0, 125);
        ASSERT_EQ(result.size(), 2);
        EXPECT_EQ(result[0], std::make_pair(static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(125)));
        EXPECT_EQ(result[1], std::make_pair(static_cast<std::uint16_t>(1), static_cast<std::uint16_t>(126)));
    }

    TEST(DiagSumCellsTest, CellsAllOnSameDiagonal) {
        // All cells returned should map to the same diagonal index.
        const DiagSum ds(kS);
        const CellVec result = ds.cells(30, 80);
        // lineIndex(30, 80) = 80 - 30 + 126 = 176
        for (const auto &[r, c] : result) {
            // For diagonal: c - r + (s-1) should be constant
            const auto d = static_cast<std::int32_t>(c) - static_cast<std::int32_t>(r) + 126;
            EXPECT_EQ(d, 176);
        }
    }

    TEST(DiagSumCellsTest, CellsConsistentForDifferentInputsOnSameDiagonal) {
        // Two cells on the same diagonal should return the same cells() vector.
        const DiagSum ds(kS);
        const CellVec a = ds.cells(0, 0);     // lineIndex = 126
        const CellVec b = ds.cells(63, 63);   // lineIndex = 126
        const CellVec c = ds.cells(126, 126); // lineIndex = 126
        EXPECT_EQ(a, b);
        EXPECT_EQ(b, c);
    }

    TEST(DiagSumCellsTest, SmallMatrixCellsMainDiagonal) {
        const DiagSum ds(3);
        // lineIndex(0,0) = 0 - 0 + 2 = 2. Main diagonal: (0,0),(1,1),(2,2). Length = 3.
        const CellVec result = ds.cells(0, 0);
        const CellVec expected = {{0, 0}, {1, 1}, {2, 2}};
        EXPECT_EQ(result, expected);
    }

    TEST(DiagSumCellsTest, SmallMatrixCellsCornerDiagonals) {
        const DiagSum ds(3);
        // lineIndex(2, 0) = 0 - 2 + 2 = 0. Single cell (2,0).
        const CellVec bottom_left = ds.cells(2, 0);
        ASSERT_EQ(bottom_left.size(), 1);
        EXPECT_EQ(bottom_left[0], std::make_pair(static_cast<std::uint16_t>(2), static_cast<std::uint16_t>(0)));

        // lineIndex(0, 2) = 2 - 0 + 2 = 4. Single cell (0,2).
        const CellVec top_right = ds.cells(0, 2);
        ASSERT_EQ(top_right.size(), 1);
        EXPECT_EQ(top_right[0], std::make_pair(static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(2)));
    }

    TEST(DiagSumCellsTest, SmallMatrixCellsSubDiagonals) {
        const DiagSum ds(3);
        // lineIndex(1, 0) = 0 - 1 + 2 = 1. Cells: (1,0),(2,1). Length = 2.
        const CellVec result1 = ds.cells(1, 0);
        const CellVec expected1 = {{1, 0}, {2, 1}};
        EXPECT_EQ(result1, expected1);

        // lineIndex(0, 1) = 1 - 0 + 2 = 3. Cells: (0,1),(1,2). Length = 2.
        const CellVec result3 = ds.cells(0, 1);
        const CellVec expected3 = {{0, 1}, {1, 2}};
        EXPECT_EQ(result3, expected3);
    }

    // =======================================================================
    // AntiDiagSum::cells() tests
    // =======================================================================

    TEST(AntiDiagSumCellsTest, CellsIndex0SingleCell) {
        // Anti-diagonal index 0 corresponds to the top-left corner cell (0, 0).
        // lineIndex(0, 0) = 0. Length = 1.
        const AntiDiagSum xs(kS);
        const CellVec result = xs.cells(0, 0);
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result[0], std::make_pair(static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(0)));
    }

    TEST(AntiDiagSumCellsTest, CellsIndex252SingleCell) {
        // Anti-diagonal index 252 corresponds to the bottom-right corner cell (126, 126).
        // lineIndex(126, 126) = 252. Length = 1.
        const AntiDiagSum xs(kS);
        const CellVec result = xs.cells(126, 126);
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result[0], std::make_pair(static_cast<std::uint16_t>(126), static_cast<std::uint16_t>(126)));
    }

    TEST(AntiDiagSumCellsTest, CellsMainAntiDiagonalIndex126) {
        // Anti-diagonal index 126 is the main anti-diagonal. lineIndex(0, 126) = 126.
        // Length = 127, cells from (0, 126) to (126, 0).
        const AntiDiagSum xs(kS);
        const CellVec result = xs.cells(0, 126);
        ASSERT_EQ(result.size(), 127);
        for (std::uint16_t i = 0; i < 127; ++i) {
            EXPECT_EQ(result[i].first, i);
            EXPECT_EQ(result[i].second, static_cast<std::uint16_t>(126 - i));
        }
    }

    TEST(AntiDiagSumCellsTest, CellsLengthMatchesLen) {
        const AntiDiagSum xs(kS);
        // Anti-diagonal index 0: cell (0, 0)
        EXPECT_EQ(xs.cells(0, 0).size(), xs.len(0));
        // Anti-diagonal index 1: cells (0, 1) and (1, 0)
        EXPECT_EQ(xs.cells(0, 1).size(), xs.len(1));
        // Anti-diagonal index 126 (main anti-diagonal)
        EXPECT_EQ(xs.cells(0, 126).size(), xs.len(126));
        // Anti-diagonal index 252: cell (126, 126)
        EXPECT_EQ(xs.cells(126, 126).size(), xs.len(252));
    }

    TEST(AntiDiagSumCellsTest, CellsIndex1TwoCells) {
        // Anti-diagonal index 1: lineIndex(r,c) = r + c = 1.
        // Valid cells: r=0 -> c=1, r=1 -> c=0. Length = 2.
        const AntiDiagSum xs(kS);
        const CellVec result = xs.cells(0, 1);
        ASSERT_EQ(result.size(), 2);
        EXPECT_EQ(result[0], std::make_pair(static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(1)));
        EXPECT_EQ(result[1], std::make_pair(static_cast<std::uint16_t>(1), static_cast<std::uint16_t>(0)));
    }

    TEST(AntiDiagSumCellsTest, CellsIndex251TwoCells) {
        // Anti-diagonal index 251: lineIndex(r,c) = r + c = 251.
        // Valid cells: r=125 -> c=126, r=126 -> c=125. Length = 2.
        const AntiDiagSum xs(kS);
        const CellVec result = xs.cells(125, 126);
        ASSERT_EQ(result.size(), 2);
        EXPECT_EQ(result[0], std::make_pair(static_cast<std::uint16_t>(125), static_cast<std::uint16_t>(126)));
        EXPECT_EQ(result[1], std::make_pair(static_cast<std::uint16_t>(126), static_cast<std::uint16_t>(125)));
    }

    TEST(AntiDiagSumCellsTest, CellsAllOnSameAntiDiagonal) {
        // All cells returned should map to the same anti-diagonal index.
        const AntiDiagSum xs(kS);
        const CellVec result = xs.cells(30, 80);
        // lineIndex(30, 80) = 110
        for (const auto &[r, c] : result) {
            EXPECT_EQ(static_cast<std::uint16_t>(r + c), 110);
        }
    }

    TEST(AntiDiagSumCellsTest, CellsConsistentForDifferentInputsOnSameAntiDiagonal) {
        // Two cells on the same anti-diagonal should return the same cells() vector.
        const AntiDiagSum xs(kS);
        const CellVec a = xs.cells(0, 126);   // lineIndex = 126
        const CellVec b = xs.cells(63, 63);   // lineIndex = 126
        const CellVec c = xs.cells(126, 0);   // lineIndex = 126
        EXPECT_EQ(a, b);
        EXPECT_EQ(b, c);
    }

    TEST(AntiDiagSumCellsTest, SmallMatrixCellsMainAntiDiagonal) {
        const AntiDiagSum xs(3);
        // lineIndex(0, 2) = 2. Main anti-diagonal: (0,2),(1,1),(2,0). Length = 3.
        const CellVec result = xs.cells(0, 2);
        const CellVec expected = {{0, 2}, {1, 1}, {2, 0}};
        EXPECT_EQ(result, expected);
    }

    TEST(AntiDiagSumCellsTest, SmallMatrixCellsCornerAntiDiagonals) {
        const AntiDiagSum xs(3);
        // lineIndex(0, 0) = 0. Single cell (0,0).
        const CellVec top_left = xs.cells(0, 0);
        ASSERT_EQ(top_left.size(), 1);
        EXPECT_EQ(top_left[0], std::make_pair(static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(0)));

        // lineIndex(2, 2) = 4. Single cell (2,2).
        const CellVec bottom_right = xs.cells(2, 2);
        ASSERT_EQ(bottom_right.size(), 1);
        EXPECT_EQ(bottom_right[0], std::make_pair(static_cast<std::uint16_t>(2), static_cast<std::uint16_t>(2)));
    }

    TEST(AntiDiagSumCellsTest, SmallMatrixCellsSubAntiDiagonals) {
        const AntiDiagSum xs(3);
        // lineIndex(1, 0) = 1. Cells: (0,1),(1,0). Length = 2.
        const CellVec result1 = xs.cells(1, 0);
        const CellVec expected1 = {{0, 1}, {1, 0}};
        EXPECT_EQ(result1, expected1);

        // lineIndex(2, 1) = 3. Cells: (1,2),(2,1). Length = 2.
        const CellVec result3 = xs.cells(2, 1);
        const CellVec expected3 = {{1, 2}, {2, 1}};
        EXPECT_EQ(result3, expected3);
    }

    // =======================================================================
    // Cross-type cells() consistency: cells() vectors cover correct coordinates
    // =======================================================================

    TEST(CrossSumCellsTest, RowAndColCellsIntersectAtGivenCell) {
        const RowSum rs(kS);
        const ColSum cs(kS);
        const CellVec row_cells = rs.cells(30, 80);
        const CellVec col_cells = cs.cells(30, 80);

        // The intersection of a row's cells and a column's cells should be exactly the cell (30, 80).
        bool found = false;
        for (const auto &rc : row_cells) {
            for (const auto &cc : col_cells) {
                if (rc == cc) {
                    EXPECT_EQ(rc.first, 30);
                    EXPECT_EQ(rc.second, 80);
                    found = true;
                }
            }
        }
        EXPECT_TRUE(found);
    }

    TEST(CrossSumCellsTest, AllFourCellsVectorsContainTheQueryCell) {
        const RowSum rs(kS);
        const ColSum cs(kS);
        const DiagSum ds(kS);
        const AntiDiagSum xs(kS);

        const auto cell = std::make_pair(static_cast<std::uint16_t>(50), static_cast<std::uint16_t>(80));

        const CellVec rv = rs.cells(50, 80);
        const CellVec cv = cs.cells(50, 80);
        const CellVec dv = ds.cells(50, 80);
        const CellVec xv = xs.cells(50, 80);

        EXPECT_NE(std::ranges::find(rv, cell), rv.end());
        EXPECT_NE(std::ranges::find(cv, cell), cv.end());
        EXPECT_NE(std::ranges::find(dv, cell), dv.end());
        EXPECT_NE(std::ranges::find(xv, cell), xv.end());
    }

    TEST(CrossSumCellsTest, CellsCoordinatesAreWithinBounds) {
        const RowSum rs(kS);
        const ColSum cs(kS);
        const DiagSum ds(kS);
        const AntiDiagSum xs(kS);

        auto check_bounds = [](const CellVec &cells, std::uint16_t s) {
            for (const auto &[r, c] : cells) {
                EXPECT_LT(r, s);
                EXPECT_LT(c, s);
            }
        };

        check_bounds(rs.cells(0, 0), kS);
        check_bounds(cs.cells(126, 126), kS);
        check_bounds(ds.cells(0, 126), kS);
        check_bounds(ds.cells(126, 0), kS);
        check_bounds(xs.cells(0, 0), kS);
        check_bounds(xs.cells(126, 126), kS);
    }

    TEST(CrossSumCellsTest, DiagAndAntiDiagCellsIntersectAtQueryCell) {
        const DiagSum ds(kS);
        const AntiDiagSum xs(kS);
        const CellVec diag_cells = ds.cells(50, 80);
        const CellVec anti_cells = xs.cells(50, 80);

        // The intersection should contain the query cell (50, 80).
        const auto target = std::make_pair(static_cast<std::uint16_t>(50), static_cast<std::uint16_t>(80));
        bool found = false;
        for (const auto &dc : diag_cells) {
            for (const auto &ac : anti_cells) {
                if (dc == ac && dc == target) {
                    found = true;
                }
            }
        }
        EXPECT_TRUE(found);
    }

} // namespace
