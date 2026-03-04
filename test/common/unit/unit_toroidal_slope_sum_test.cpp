/**
 * @file unit_toroidal_slope_sum_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Unit tests for ToroidalSlopeSum: slopeLineIndex, lineIndex, len, cells, and partition axiom.
 */
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

#include "common/CrossSum/ToroidalSlopeSum.h"

namespace {

    using crsce::common::ToroidalSlopeSum;

    using CellVec = std::vector<std::pair<std::uint16_t, std::uint16_t>>;

    constexpr std::uint16_t kS = 511;

    /// The four slopes used in the B.5 partitions.
    constexpr std::array<std::uint16_t, 4> kSlopes = {256, 255, 2, 509};

    // =======================================================================
    // Helper: compute expected line index in test code independently.
    // =======================================================================
    std::uint16_t expectedLineIndex(const std::uint16_t r, const std::uint16_t c,
                                    const std::uint16_t slope, const std::uint16_t s) {
        const auto si = static_cast<std::int32_t>(s);
        const auto val = static_cast<std::int32_t>(c)
                       - (static_cast<std::int32_t>(slope) * static_cast<std::int32_t>(r));
        return static_cast<std::uint16_t>(((val % si) + si) % si);
    }

    // =======================================================================
    // 1. slopeLineIndex static method
    // =======================================================================

    /**
     * @name SlopeLineIndexSlope256
     * @brief Verify slopeLineIndex for slope 256 with known (r,c) pairs.
     */
    TEST(ToroidalSlopeSumTest, SlopeLineIndexSlope256) {
        // k = ((c - 256*r) % 511 + 511) % 511
        // (0, 0) -> 0
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(0, 0, 256, kS), 0);
        // (0, 100) -> 100
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(0, 100, 256, kS), 100);
        // (1, 0) -> ((-256) % 511 + 511) % 511 = (511 - 256) = 255
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(1, 0, 256, kS), 255);
        // (2, 0) -> ((-512) % 511 + 511) % 511 = ((-1) % 511 + 511) % 511 = 510
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(2, 0, 256, kS), 510);
        // (1, 256) -> ((256 - 256) % 511 + 511) % 511 = 0
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(1, 256, 256, kS), 0);
        // (510, 510) -> ((510 - 256*510) % 511 + 511) % 511
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(510, 510, 256, kS),
                  expectedLineIndex(510, 510, 256, kS));
    }

    /**
     * @name SlopeLineIndexSlope255
     * @brief Verify slopeLineIndex for slope 255 with known (r,c) pairs.
     */
    TEST(ToroidalSlopeSumTest, SlopeLineIndexSlope255) {
        // (0, 0) -> 0
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(0, 0, 255, kS), 0);
        // (1, 0) -> ((-255) % 511 + 511) % 511 = 256
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(1, 0, 255, kS), 256);
        // (1, 255) -> ((255 - 255) % 511 + 511) % 511 = 0
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(1, 255, 255, kS), 0);
        // (2, 1) -> ((1 - 510) % 511 + 511) % 511 = ((-509) % 511 + 511) % 511 = 2
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(2, 1, 255, kS), 2);
        // General case
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(300, 400, 255, kS),
                  expectedLineIndex(300, 400, 255, kS));
    }

    /**
     * @name SlopeLineIndexSlope2
     * @brief Verify slopeLineIndex for slope 2 with known (r,c) pairs.
     */
    TEST(ToroidalSlopeSumTest, SlopeLineIndexSlope2) {
        // (0, 0) -> 0
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(0, 0, 2, kS), 0);
        // (1, 0) -> ((-2) % 511 + 511) % 511 = 509
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(1, 0, 2, kS), 509);
        // (0, 2) -> 2
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(0, 2, 2, kS), 2);
        // (1, 2) -> ((2 - 2) % 511 + 511) % 511 = 0
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(1, 2, 2, kS), 0);
        // (255, 100) -> ((100 - 510) % 511 + 511) % 511 = ((-410) % 511 + 511) % 511 = 101
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(255, 100, 2, kS), 101);
        // General case
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(400, 50, 2, kS),
                  expectedLineIndex(400, 50, 2, kS));
    }

    /**
     * @name SlopeLineIndexSlope509
     * @brief Verify slopeLineIndex for slope 509 with known (r,c) pairs.
     */
    TEST(ToroidalSlopeSumTest, SlopeLineIndexSlope509) {
        // (0, 0) -> 0
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(0, 0, 509, kS), 0);
        // (1, 0) -> ((-509) % 511 + 511) % 511 = 2
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(1, 0, 509, kS), 2);
        // (0, 509) -> 509
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(0, 509, 509, kS), 509);
        // (1, 509) -> ((509 - 509) % 511 + 511) % 511 = 0
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(1, 509, 509, kS), 0);
        // General case
        EXPECT_EQ(ToroidalSlopeSum::slopeLineIndex(100, 200, 509, kS),
                  expectedLineIndex(100, 200, 509, kS));
    }

    // =======================================================================
    // 2. lineIndex instance method matches slopeLineIndex
    // =======================================================================

    /**
     * @name LineIndexMatchesSlopeLineIndex
     * @brief Verify that the instance lineIndex method (via set/get) agrees with slopeLineIndex.
     *
     * Since lineIndex is protected, we verify indirectly: set a value at (r,c), then confirm
     * getByIndex at the slopeLineIndex position reflects the accumulation.
     */
    TEST(ToroidalSlopeSumTest, LineIndexMatchesSlopeLineIndex) {
        for (const std::uint16_t slope : kSlopes) {
            ToroidalSlopeSum ts(kS, slope);

            // Set a value at a sample cell and check getByIndex at the expected line.
            constexpr std::uint16_t r = 123;
            constexpr std::uint16_t c = 456;
            const std::uint16_t expectedK = ToroidalSlopeSum::slopeLineIndex(r, c, slope, kS);

            ts.set(r, c, 1);
            EXPECT_EQ(ts.getByIndex(expectedK), 1)
                << "slope=" << slope << " r=" << r << " c=" << c << " expectedK=" << expectedK;

            // A different cell on the same line should accumulate into the same index.
            // Cell (t, (k + slope*t) % s) lies on line k. Pick t = 1.
            const auto c2 = static_cast<std::uint16_t>((expectedK + static_cast<std::uint32_t>(slope)) % kS);
            ts.set(1, c2, 1);
            EXPECT_EQ(ts.getByIndex(expectedK), 2)
                << "slope=" << slope << " second cell (1, " << c2 << ") should share line " << expectedK;
        }
    }

    /**
     * @name LineIndexMatchesSlopeLineIndexBoundary
     * @brief Verify lineIndex at boundary coordinates (0,0), (0,510), (510,0), (510,510).
     */
    TEST(ToroidalSlopeSumTest, LineIndexMatchesSlopeLineIndexBoundary) {
        constexpr std::array<std::pair<std::uint16_t, std::uint16_t>, 4> corners = {{
            {0, 0}, {0, 510}, {510, 0}, {510, 510}
        }};

        for (const std::uint16_t slope : kSlopes) {
            for (const auto &[r, c] : corners) {
                ToroidalSlopeSum ts(kS, slope);
                const std::uint16_t expectedK = ToroidalSlopeSum::slopeLineIndex(r, c, slope, kS);
                ts.set(r, c, 1);
                EXPECT_EQ(ts.getByIndex(expectedK), 1)
                    << "slope=" << slope << " corner=(" << r << "," << c << ")";
            }
        }
    }

    // =======================================================================
    // 3. len(k) always returns 511
    // =======================================================================

    /**
     * @name LenAlwaysReturns511
     * @brief Verify len(k) returns kS (511) for all valid k in [0, 510].
     */
    TEST(ToroidalSlopeSumTest, LenAlwaysReturns511) {
        for (const std::uint16_t slope : kSlopes) {
            const ToroidalSlopeSum ts(kS, slope);
            for (std::uint16_t k = 0; k < kS; ++k) {
                EXPECT_EQ(ts.len(k), kS)
                    << "slope=" << slope << " k=" << k;
            }
        }
    }

    /**
     * @name LenBoundaryValues
     * @brief Verify len at boundary line indices 0 and 510.
     */
    TEST(ToroidalSlopeSumTest, LenBoundaryValues) {
        for (const std::uint16_t slope : kSlopes) {
            const ToroidalSlopeSum ts(kS, slope);
            EXPECT_EQ(ts.len(0), kS) << "slope=" << slope;
            EXPECT_EQ(ts.len(510), kS) << "slope=" << slope;
        }
    }

    // =======================================================================
    // 4. cells(k) returns exactly 511 cells, all unique, all in bounds
    // =======================================================================

    /**
     * @name CellsReturns511UniqueCellsInBounds
     * @brief For each slope and several line indices, cells() returns 511 unique in-bounds cells.
     */
    TEST(ToroidalSlopeSumTest, CellsReturns511UniqueCellsInBounds) {
        constexpr std::array<std::uint16_t, 6> testLines = {0, 1, 100, 255, 400, 510};

        for (const std::uint16_t slope : kSlopes) {
            const ToroidalSlopeSum ts(kS, slope);

            for (const std::uint16_t k : testLines) {
                // To call cells(r,c), we need a cell on line k.
                // Cell (0, k) has lineIndex = ((k - slope*0) % 511 + 511) % 511 = k.
                const CellVec result = ts.cells(0, k);

                // Exactly 511 cells.
                ASSERT_EQ(result.size(), kS)
                    << "slope=" << slope << " k=" << k;

                // All unique and in bounds.
                std::set<std::pair<std::uint16_t, std::uint16_t>> unique_cells;
                for (const auto &[r, c] : result) {
                    EXPECT_LT(r, kS) << "slope=" << slope << " k=" << k << " r=" << r;
                    EXPECT_LT(c, kS) << "slope=" << slope << " k=" << k << " c=" << c;
                    unique_cells.insert({r, c});
                }
                EXPECT_EQ(unique_cells.size(), kS)
                    << "slope=" << slope << " k=" << k << " cells not all unique";
            }
        }
    }

    // =======================================================================
    // 5. cells(k) -- every cell maps back to line k via lineIndex
    // =======================================================================

    /**
     * @name CellsMapBackToCorrectLine
     * @brief Every cell returned by cells() for line k maps back to k via slopeLineIndex.
     */
    TEST(ToroidalSlopeSumTest, CellsMapBackToCorrectLine) {
        constexpr std::array<std::uint16_t, 6> testLines = {0, 1, 50, 255, 400, 510};

        for (const std::uint16_t slope : kSlopes) {
            const ToroidalSlopeSum ts(kS, slope);

            for (const std::uint16_t k : testLines) {
                // Cell (0, k) is on line k for any slope (since slope*0 = 0).
                const CellVec result = ts.cells(0, k);

                for (const auto &[r, c] : result) {
                    const std::uint16_t computedK = ToroidalSlopeSum::slopeLineIndex(r, c, slope, kS);
                    EXPECT_EQ(computedK, k)
                        << "slope=" << slope << " k=" << k << " cell=(" << r << "," << c << ")";
                }
            }
        }
    }

    /**
     * @name CellsConsistentForDifferentInputsOnSameLine
     * @brief Two cells on the same line produce identical cells() vectors.
     */
    TEST(ToroidalSlopeSumTest, CellsConsistentForDifferentInputsOnSameLine) {
        for (const std::uint16_t slope : kSlopes) {
            const ToroidalSlopeSum ts(kS, slope);

            // Pick line k = 42. Cell (0, 42) is on this line.
            // Cell (1, (42 + slope) % 511) is also on this line.
            constexpr std::uint16_t k = 42;
            const auto c2 = static_cast<std::uint16_t>((k + static_cast<std::uint32_t>(slope)) % kS);

            const CellVec a = ts.cells(0, k);
            const CellVec b = ts.cells(1, c2);
            EXPECT_EQ(a, b) << "slope=" << slope;
        }
    }

    // =======================================================================
    // 6. Partition axiom: every cell belongs to exactly one line per slope
    // =======================================================================

    /**
     * @name PartitionAxiomEveryCellOnExactlyOneLine
     * @brief For each slope, every cell (r,c) in [0,511)^2 belongs to exactly one line.
     *
     * We verify by checking that the union of all 511 lines contains exactly 511*511 = 261121
     * cells and that no cell appears in two different lines.
     */
    TEST(ToroidalSlopeSumTest, PartitionAxiomEveryCellOnExactlyOneLine) {
        for (const std::uint16_t slope : kSlopes) {
            // Build a map: for each cell (r,c), record which line index it maps to.
            // If the partition is correct, every cell maps to exactly one line.
            std::vector<std::uint16_t> lineOf(static_cast<std::size_t>(kS) * kS);

            for (std::uint16_t r = 0; r < kS; ++r) {
                for (std::uint16_t c = 0; c < kS; ++c) {
                    lineOf[(static_cast<std::size_t>(r) * kS) + c] =
                        ToroidalSlopeSum::slopeLineIndex(r, c, slope, kS);
                }
            }

            // Count cells per line. Each of the 511 lines should have exactly 511 cells.
            std::vector<std::uint16_t> cellsPerLine(kS, 0);
            for (std::uint16_t r = 0; r < kS; ++r) {
                for (std::uint16_t c = 0; c < kS; ++c) {
                    const std::uint16_t k = lineOf[(static_cast<std::size_t>(r) * kS) + c];
                    ASSERT_LT(k, kS) << "slope=" << slope << " cell=(" << r << "," << c << ")";
                    ++cellsPerLine[k];
                }
            }

            for (std::uint16_t k = 0; k < kS; ++k) {
                EXPECT_EQ(cellsPerLine[k], kS)
                    << "slope=" << slope << " line=" << k << " has " << cellsPerLine[k] << " cells";
            }
        }
    }

    /**
     * @name PartitionAxiomCellsEnumerationMatchesSlopeLineIndex
     * @brief The cells enumerated by cells() for a line match exactly the cells that slopeLineIndex
     *        maps to that line, verifying consistency between cells() and the partition formula.
     */
    TEST(ToroidalSlopeSumTest, PartitionAxiomCellsEnumerationMatchesSlopeLineIndex) {
        // Test a single slope exhaustively (all 511 lines). Use slope 2 as representative.
        constexpr std::uint16_t slope = 2;
        const ToroidalSlopeSum ts(kS, slope);

        for (std::uint16_t k = 0; k < kS; ++k) {
            // Enumerate cells for line k via cells(0, k).
            const CellVec enumerated = ts.cells(0, k);

            // Build expected set: all (r,c) where slopeLineIndex(r,c,slope,kS) == k.
            std::set<std::pair<std::uint16_t, std::uint16_t>> expected;
            for (std::uint16_t t = 0; t < kS; ++t) {
                const auto col = static_cast<std::uint16_t>(
                    (static_cast<std::uint32_t>(k) + static_cast<std::uint32_t>(slope) * t) % kS);
                expected.insert({t, col});
            }

            // Convert enumerated to set for comparison.
            const std::set<std::pair<std::uint16_t, std::uint16_t>> enumeratedSet(enumerated.begin(), enumerated.end());

            EXPECT_EQ(enumeratedSet, expected) << "slope=" << slope << " k=" << k;
        }
    }

    // =======================================================================
    // Size and basic construction
    // =======================================================================

    /**
     * @name SizeEquals511
     * @brief ToroidalSlopeSum has 511 lines (size() == kS).
     */
    TEST(ToroidalSlopeSumTest, SizeEquals511) {
        for (const std::uint16_t slope : kSlopes) {
            const ToroidalSlopeSum ts(kS, slope);
            EXPECT_EQ(ts.size(), kS) << "slope=" << slope;
        }
    }

    /**
     * @name InitialSumsAreZero
     * @brief All sums are initially zero after construction.
     */
    TEST(ToroidalSlopeSumTest, InitialSumsAreZero) {
        for (const std::uint16_t slope : kSlopes) {
            const ToroidalSlopeSum ts(kS, slope);
            for (std::uint16_t k = 0; k < kS; ++k) {
                EXPECT_EQ(ts.getByIndex(k), 0)
                    << "slope=" << slope << " k=" << k;
            }
        }
    }

} // namespace
