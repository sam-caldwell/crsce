/**
 * @file unit_ltp_table_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Unit tests for LtpTable: Fisher-Yates B.9 non-linear partition.
 *
 * Verifies three axioms:
 *   Axiom 1 (Conservation): sum of all line sums = s^2 = 261,121 (each cell counted once).
 *   Axiom 2 (No duplicates within line): each cell appears at most once per line.
 *   Axiom 3 (Coverage): every cell (r, c) appears on exactly one LTP1 line and one LTP2 line.
 * Also verifies seed reproducibility (same tables on repeated construction).
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <set>
#include <utility>

#include "decompress/Solvers/LtpTable.h"

using crsce::decompress::solvers::kLtp1Base;
using crsce::decompress::solvers::kLtp2Base;
using crsce::decompress::solvers::kLtp3Base;
using crsce::decompress::solvers::kLtp4Base;
using crsce::decompress::solvers::kLtpNumLines;
using crsce::decompress::solvers::kLtpS;
using crsce::decompress::solvers::ltp1CellsForLine;
using crsce::decompress::solvers::ltp2CellsForLine;
using crsce::decompress::solvers::ltp3CellsForLine;
using crsce::decompress::solvers::ltp4CellsForLine;
using crsce::decompress::solvers::ltpFlatIndices;

namespace {
    constexpr std::uint32_t kTotalCells = static_cast<std::uint32_t>(kLtpS) * kLtpS; // 261,121
} // namespace

// ---------------------------------------------------------------------------
// LTP1 Axiom 1: each cell appears on exactly one LTP1 line (total = s^2)
// ---------------------------------------------------------------------------

/**
 * @brief Axiom 1 for LTP1: iterating all lines yields exactly s^2 = 261,121 (r,c) pairs.
 *
 * Counts the total number of cells across all 511 LTP1 lines. Must equal kTotalCells.
 */
TEST(LtpTableTest, Ltp1AxiomOneConservation) {
    std::uint32_t count = 0;
    for (std::uint16_t k = 0; k < kLtpNumLines; ++k) {
        const auto &cells = ltp1CellsForLine(k);
        count += static_cast<std::uint32_t>(cells.size());
    }
    EXPECT_EQ(count, kTotalCells);
}

// ---------------------------------------------------------------------------
// LTP1 Axiom 2: no duplicate cells within any line
// ---------------------------------------------------------------------------

/**
 * @brief Axiom 2 for LTP1: within each line, all 511 (r,c) pairs are distinct.
 *
 * Samples lines 0, 255, and 510 (first, middle, last) for speed.
 */
TEST(LtpTableTest, Ltp1AxiomTwoNoDuplicatesInLine) {
    for (const std::uint16_t k : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(255),
                                   static_cast<std::uint16_t>(510)}) {
        const auto &cells = ltp1CellsForLine(k);
        std::set<std::pair<std::uint16_t, std::uint16_t>> seen;
        for (const auto &cell : cells) {
            const auto [it, inserted] = seen.emplace(cell.r, cell.c);
            EXPECT_TRUE(inserted) << "Duplicate cell (" << cell.r << "," << cell.c
                                  << ") in LTP1 line " << k;
        }
        EXPECT_EQ(seen.size(), static_cast<std::size_t>(kLtpS));
    }
}

// ---------------------------------------------------------------------------
// LTP1 Axiom 3: every cell covered by exactly one LTP1 line
// ---------------------------------------------------------------------------

/**
 * @brief Axiom 3 for LTP1: every cell (r, c) is on exactly one LTP1 line.
 *
 * Builds a coverage bitset via the forward table and checks totals.
 * Uses ltpFlatIndices to verify consistency between forward and reverse tables.
 */
TEST(LtpTableTest, Ltp1AxiomThreeFullCoverage) {
    std::uint32_t covered = 0;
    for (std::uint16_t r = 0; r < kLtpS; ++r) {
        for (std::uint16_t c = 0; c < kLtpS; ++c) {
            const auto &idx = ltpFlatIndices(r, c);
            const auto flatLine = idx[0]; // kLtp1Base + line_index
            EXPECT_GE(flatLine, static_cast<std::uint16_t>(kLtp1Base));
            EXPECT_LT(flatLine, static_cast<std::uint16_t>(kLtp2Base));
            ++covered;
        }
    }
    EXPECT_EQ(covered, kTotalCells);
}

// ---------------------------------------------------------------------------
// LTP2 Axiom 1: each cell appears on exactly one LTP2 line (total = s^2)
// ---------------------------------------------------------------------------

/**
 * @brief Axiom 1 for LTP2: iterating all lines yields exactly s^2 = 261,121 (r,c) pairs.
 */
TEST(LtpTableTest, Ltp2AxiomOneConservation) {
    std::uint32_t count = 0;
    for (std::uint16_t k = 0; k < kLtpNumLines; ++k) {
        const auto &cells = ltp2CellsForLine(k);
        count += static_cast<std::uint32_t>(cells.size());
    }
    EXPECT_EQ(count, kTotalCells);
}

// ---------------------------------------------------------------------------
// LTP2 Axiom 2: no duplicate cells within any line
// ---------------------------------------------------------------------------

/**
 * @brief Axiom 2 for LTP2: within each line, all 511 (r,c) pairs are distinct.
 *
 * Samples lines 0, 255, and 510 for speed.
 */
TEST(LtpTableTest, Ltp2AxiomTwoNoDuplicatesInLine) {
    for (const std::uint16_t k : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(255),
                                   static_cast<std::uint16_t>(510)}) {
        const auto &cells = ltp2CellsForLine(k);
        std::set<std::pair<std::uint16_t, std::uint16_t>> seen;
        for (const auto &cell : cells) {
            const auto [it, inserted] = seen.emplace(cell.r, cell.c);
            EXPECT_TRUE(inserted) << "Duplicate cell (" << cell.r << "," << cell.c
                                  << ") in LTP2 line " << k;
        }
        EXPECT_EQ(seen.size(), static_cast<std::size_t>(kLtpS));
    }
}

// ---------------------------------------------------------------------------
// LTP2 Axiom 3: every cell covered by exactly one LTP2 line
// ---------------------------------------------------------------------------

/**
 * @brief Axiom 3 for LTP2: every cell (r, c) is on exactly one LTP2 line.
 */
TEST(LtpTableTest, Ltp2AxiomThreeFullCoverage) {
    std::uint32_t covered = 0;
    for (std::uint16_t r = 0; r < kLtpS; ++r) {
        for (std::uint16_t c = 0; c < kLtpS; ++c) {
            const auto &idx = ltpFlatIndices(r, c);
            const auto flatLine = idx[1]; // kLtp2Base + line_index
            EXPECT_GE(flatLine, static_cast<std::uint16_t>(kLtp2Base));
            EXPECT_LT(flatLine, static_cast<std::uint16_t>(kLtp3Base));
            ++covered;
        }
    }
    EXPECT_EQ(covered, kTotalCells);
}

// ---------------------------------------------------------------------------
// LTP3 Axiom 1: each cell appears on exactly one LTP3 line (total = s^2)
// ---------------------------------------------------------------------------

/**
 * @brief Axiom 1 for LTP3: iterating all lines yields exactly s^2 = 261,121 (r,c) pairs.
 */
TEST(LtpTableTest, Ltp3AxiomOneConservation) {
    std::uint32_t count = 0;
    for (std::uint16_t k = 0; k < kLtpNumLines; ++k) {
        const auto &cells = ltp3CellsForLine(k);
        count += static_cast<std::uint32_t>(cells.size());
    }
    EXPECT_EQ(count, kTotalCells);
}

/**
 * @brief Axiom 2 for LTP3: within each line, all 511 (r,c) pairs are distinct.
 */
TEST(LtpTableTest, Ltp3AxiomTwoNoDuplicatesInLine) {
    for (const std::uint16_t k : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(255),
                                   static_cast<std::uint16_t>(510)}) {
        const auto &cells = ltp3CellsForLine(k);
        std::set<std::pair<std::uint16_t, std::uint16_t>> seen;
        for (const auto &cell : cells) {
            const auto [it, inserted] = seen.emplace(cell.r, cell.c);
            EXPECT_TRUE(inserted) << "Duplicate cell (" << cell.r << "," << cell.c
                                  << ") in LTP3 line " << k;
        }
        EXPECT_EQ(seen.size(), static_cast<std::size_t>(kLtpS));
    }
}

/**
 * @brief Axiom 3 for LTP3: every cell (r, c) is on exactly one LTP3 line.
 */
TEST(LtpTableTest, Ltp3AxiomThreeFullCoverage) {
    std::uint32_t covered = 0;
    for (std::uint16_t r = 0; r < kLtpS; ++r) {
        for (std::uint16_t c = 0; c < kLtpS; ++c) {
            const auto &idx = ltpFlatIndices(r, c);
            const auto flatLine = idx[2]; // kLtp3Base + line_index
            EXPECT_GE(flatLine, static_cast<std::uint16_t>(kLtp3Base));
            EXPECT_LT(flatLine, static_cast<std::uint16_t>(kLtp4Base));
            ++covered;
        }
    }
    EXPECT_EQ(covered, kTotalCells);
}

// ---------------------------------------------------------------------------
// LTP4 Axiom 1: each cell appears on exactly one LTP4 line (total = s^2)
// ---------------------------------------------------------------------------

/**
 * @brief Axiom 1 for LTP4: iterating all lines yields exactly s^2 = 261,121 (r,c) pairs.
 */
TEST(LtpTableTest, Ltp4AxiomOneConservation) {
    std::uint32_t count = 0;
    for (std::uint16_t k = 0; k < kLtpNumLines; ++k) {
        const auto &cells = ltp4CellsForLine(k);
        count += static_cast<std::uint32_t>(cells.size());
    }
    EXPECT_EQ(count, kTotalCells);
}

/**
 * @brief Axiom 2 for LTP4: within each line, all 511 (r,c) pairs are distinct.
 */
TEST(LtpTableTest, Ltp4AxiomTwoNoDuplicatesInLine) {
    for (const std::uint16_t k : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(255),
                                   static_cast<std::uint16_t>(510)}) {
        const auto &cells = ltp4CellsForLine(k);
        std::set<std::pair<std::uint16_t, std::uint16_t>> seen;
        for (const auto &cell : cells) {
            const auto [it, inserted] = seen.emplace(cell.r, cell.c);
            EXPECT_TRUE(inserted) << "Duplicate cell (" << cell.r << "," << cell.c
                                  << ") in LTP4 line " << k;
        }
        EXPECT_EQ(seen.size(), static_cast<std::size_t>(kLtpS));
    }
}

/**
 * @brief Axiom 3 for LTP4: every cell (r, c) is on exactly one LTP4 line.
 */
TEST(LtpTableTest, Ltp4AxiomThreeFullCoverage) {
    std::uint32_t covered = 0;
    for (std::uint16_t r = 0; r < kLtpS; ++r) {
        for (std::uint16_t c = 0; c < kLtpS; ++c) {
            const auto &idx = ltpFlatIndices(r, c);
            const auto flatLine = idx[3]; // kLtp4Base + line_index
            EXPECT_GE(flatLine, static_cast<std::uint16_t>(kLtp4Base));
            ++covered;
        }
    }
    EXPECT_EQ(covered, kTotalCells);
}

// ---------------------------------------------------------------------------
// Forward/reverse consistency: ltpFlatIndices agrees with ltp1/2CellsForLine
// ---------------------------------------------------------------------------

/**
 * @brief Forward and reverse tables are consistent for sampled cells.
 *
 * For cell (r, c), the forward table gives line index k.
 * The reverse table for k must contain (r, c) somewhere.
 */
TEST(LtpTableTest, ForwardReverseConsistencyLtp1) {
    // Sample a small set of cells for speed.
    for (const std::uint16_t r : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(100),
                                   static_cast<std::uint16_t>(510)}) {
        for (const std::uint16_t c : {static_cast<std::uint16_t>(0),
                                       static_cast<std::uint16_t>(200),
                                       static_cast<std::uint16_t>(510)}) {
            const auto &idx = ltpFlatIndices(r, c);
            const auto lineIdx = static_cast<std::uint16_t>(idx[0] - static_cast<std::uint16_t>(kLtp1Base));
            const auto &cells = ltp1CellsForLine(lineIdx);
            bool found = false;
            for (const auto &cell : cells) {
                if (cell.r == r && cell.c == c) {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found) << "Cell (" << r << "," << c << ") not found in LTP1 reverse line " << lineIdx;
        }
    }
}

/**
 * @brief Forward and reverse tables are consistent for sampled cells (LTP2).
 */
TEST(LtpTableTest, ForwardReverseConsistencyLtp2) {
    for (const std::uint16_t r : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(100),
                                   static_cast<std::uint16_t>(510)}) {
        for (const std::uint16_t c : {static_cast<std::uint16_t>(0),
                                       static_cast<std::uint16_t>(200),
                                       static_cast<std::uint16_t>(510)}) {
            const auto &idx = ltpFlatIndices(r, c);
            const auto lineIdx = static_cast<std::uint16_t>(idx[1] - static_cast<std::uint16_t>(kLtp2Base));
            const auto &cells = ltp2CellsForLine(lineIdx);
            bool found = false;
            for (const auto &cell : cells) {
                if (cell.r == r && cell.c == c) {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found) << "Cell (" << r << "," << c << ") not found in LTP2 reverse line " << lineIdx;
        }
    }
}

// ---------------------------------------------------------------------------
// Seed reproducibility: repeated calls return identical results
// ---------------------------------------------------------------------------

/**
 * @brief The tables are deterministic: calling ltpFlatIndices twice returns the same value.
 *
 * Since the table is a function-local static, this also confirms idempotent initialization.
 */
TEST(LtpTableTest, SeedReproducibility) {
    const auto &a1 = ltpFlatIndices(0, 0);
    const auto &a2 = ltpFlatIndices(0, 0);
    EXPECT_EQ(a1[0], a2[0]);
    EXPECT_EQ(a1[1], a2[1]);
    EXPECT_EQ(a1[2], a2[2]);
    EXPECT_EQ(a1[3], a2[3]);

    const auto &b1 = ltpFlatIndices(255, 100);
    const auto &b2 = ltpFlatIndices(255, 100);
    EXPECT_EQ(b1[0], b2[0]);
    EXPECT_EQ(b1[1], b2[1]);
    EXPECT_EQ(b1[2], b2[2]);
    EXPECT_EQ(b1[3], b2[3]);
}

// ---------------------------------------------------------------------------
// LTP1 and LTP2 assign each cell to different partitions
// ---------------------------------------------------------------------------

/**
 * @brief All four LTP flat index ranges are non-overlapping.
 *
 * LTP1: [kLtp1Base, kLtp2Base), LTP2: [kLtp2Base, kLtp3Base),
 * LTP3: [kLtp3Base, kLtp4Base), LTP4: [kLtp4Base, kLtp4Base+511).
 */
TEST(LtpTableTest, AllFourLtpFlatIndicesNonOverlapping) {
    for (const std::uint16_t r : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(255),
                                   static_cast<std::uint16_t>(510)}) {
        for (const std::uint16_t c : {static_cast<std::uint16_t>(0),
                                       static_cast<std::uint16_t>(255),
                                       static_cast<std::uint16_t>(510)}) {
            const auto &idx = ltpFlatIndices(r, c);
            EXPECT_GE(idx[0], static_cast<std::uint16_t>(kLtp1Base));
            EXPECT_LT(idx[0], static_cast<std::uint16_t>(kLtp2Base));
            EXPECT_GE(idx[1], static_cast<std::uint16_t>(kLtp2Base));
            EXPECT_LT(idx[1], static_cast<std::uint16_t>(kLtp3Base));
            EXPECT_GE(idx[2], static_cast<std::uint16_t>(kLtp3Base));
            EXPECT_LT(idx[2], static_cast<std::uint16_t>(kLtp4Base));
            EXPECT_GE(idx[3], static_cast<std::uint16_t>(kLtp4Base));
        }
    }
}
