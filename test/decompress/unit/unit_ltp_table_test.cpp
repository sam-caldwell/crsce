/**
 * @file unit_ltp_table_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Unit tests for LtpTable: B.57 full-coverage uniform-127 LTP partitions.
 *
 * B.57 invariants verified:
 *   Axiom 1 (Full coverage): sum of all line lengths per sub-table = 16,129 = 127*127.
 *   Axiom 2 (No duplicates): each cell appears exactly once per sub-table.
 *   Axiom 3 (Uniform lengths): ltp_len(k) = 127 for all k in [0, 126].
 *   Coverage: every cell (r,c) has ltpMembership count exactly 2.
 *   Forward/reverse consistency: ltpMembership flat index resolves to the correct reverse table.
 *   Reproducibility: ltpMembership is deterministic (static table, idempotent).
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <set>
#include <utility>

#include "decompress/Solvers/LtpTable.h"

using crsce::decompress::solvers::kLtp1Base;
using crsce::decompress::solvers::kLtp2Base;
using crsce::decompress::solvers::kLtpNumLines;
using crsce::decompress::solvers::kLtpS;
using crsce::decompress::solvers::ltp1CellsForLine;
using crsce::decompress::solvers::ltp2CellsForLine;
using crsce::decompress::solvers::ltpLineLen;
using crsce::decompress::solvers::ltpMembership;

namespace {
    /// sum_{k=0}^{126} ltp_len(k) = 127*127 = 16,129 (full matrix coverage)
    constexpr std::uint32_t kCellsPerSubTable = 16129U;
    constexpr std::uint32_t kTotalCells = static_cast<std::uint32_t>(kLtpS) * kLtpS; // 16,129
} // namespace

// ---------------------------------------------------------------------------
// Axiom 3: ltpLineLen returns correct values
// ---------------------------------------------------------------------------

/**
 * @brief ltpLineLen(k) = kLtpS (127) for all k in [0, 126] (B.57 uniform-127).
 *
 * All 127 lines have exactly 127 cells, independent of k.
 */
TEST(LtpTableTest, LtpLineLenBoundaryAndPeak) {
    constexpr auto kExpected = static_cast<std::uint32_t>(kLtpS);
    EXPECT_EQ(ltpLineLen(0),   kExpected);
    EXPECT_EQ(ltpLineLen(1),   kExpected);
    EXPECT_EQ(ltpLineLen(62),  kExpected);
    EXPECT_EQ(ltpLineLen(63),  kExpected);
    EXPECT_EQ(ltpLineLen(64),  kExpected);
    EXPECT_EQ(ltpLineLen(125), kExpected);
    EXPECT_EQ(ltpLineLen(126), kExpected);
}

/**
 * @brief Sum of all ltpLineLen values = 16,129 (127 lines * 127 cells each, B.57 uniform).
 */
TEST(LtpTableTest, LtpLineLenSumEqualsMatrixSize) {
    std::uint32_t total = 0;
    for (std::uint16_t k = 0; k < kLtpNumLines; ++k) {
        total += ltpLineLen(k);
    }
    EXPECT_EQ(total, kCellsPerSubTable);
}

// ---------------------------------------------------------------------------
// LTP1 Axiom 1: sum of all line lengths = kCellsPerSubTable
// ---------------------------------------------------------------------------

/**
 * @brief Axiom 1 for LTP1: total cells across all 127 LTP1 lines = 16,129.
 */
TEST(LtpTableTest, Ltp1AxiomOneConservation) {
    std::uint32_t count = 0;
    for (std::uint16_t k = 0; k < kLtpNumLines; ++k) {
        const auto cells = ltp1CellsForLine(k);
        count += static_cast<std::uint32_t>(cells.size());
    }
    EXPECT_EQ(count, kCellsPerSubTable);
}

// ---------------------------------------------------------------------------
// LTP1 Axiom 2: no duplicate cells within any line; line size = ltpLineLen(k)
// ---------------------------------------------------------------------------

/**
 * @brief Axiom 2 for LTP1: each line has exactly ltpLineLen(k) distinct cells.
 *
 * Samples lines 0, 63, and 126 (first, mid, last) for speed.
 */
TEST(LtpTableTest, Ltp1AxiomTwoNoDuplicatesInLine) {
    for (const std::uint16_t k : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(63),
                                   static_cast<std::uint16_t>(126)}) {
        const auto cells = ltp1CellsForLine(k);
        EXPECT_EQ(cells.size(), static_cast<std::size_t>(ltpLineLen(k)));
        std::set<std::pair<std::uint16_t, std::uint16_t>> seen;
        for (const auto &cell : cells) {
            const auto [it, inserted] = seen.emplace(cell.r, cell.c);
            EXPECT_TRUE(inserted) << "Duplicate cell (" << cell.r << "," << cell.c
                                  << ") in LTP1 line " << k;
        }
    }
}

// ---------------------------------------------------------------------------
// LTP2 Axiom 1: sum of all line lengths = kCellsPerSubTable
// ---------------------------------------------------------------------------

/**
 * @brief Axiom 1 for LTP2: total cells across all 127 LTP2 lines = 16,129.
 */
TEST(LtpTableTest, Ltp2AxiomOneConservation) {
    std::uint32_t count = 0;
    for (std::uint16_t k = 0; k < kLtpNumLines; ++k) {
        const auto cells = ltp2CellsForLine(k);
        count += static_cast<std::uint32_t>(cells.size());
    }
    EXPECT_EQ(count, kCellsPerSubTable);
}

// ---------------------------------------------------------------------------
// LTP2 Axiom 2: no duplicate cells within any line
// ---------------------------------------------------------------------------

/**
 * @brief Axiom 2 for LTP2: each sampled line has exactly ltpLineLen(k) distinct cells.
 */
TEST(LtpTableTest, Ltp2AxiomTwoNoDuplicatesInLine) {
    for (const std::uint16_t k : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(63),
                                   static_cast<std::uint16_t>(126)}) {
        const auto cells = ltp2CellsForLine(k);
        EXPECT_EQ(cells.size(), static_cast<std::size_t>(ltpLineLen(k)));
        std::set<std::pair<std::uint16_t, std::uint16_t>> seen;
        for (const auto &cell : cells) {
            const auto [it, inserted] = seen.emplace(cell.r, cell.c);
            EXPECT_TRUE(inserted) << "Duplicate cell (" << cell.r << "," << cell.c
                                  << ") in LTP2 line " << k;
        }
    }
}

// B.57: LTP3-6 tests removed (only 2 sub-tables).

// ---------------------------------------------------------------------------
// Coverage: every cell has membership count exactly 2
// ---------------------------------------------------------------------------

/**
 * @brief Every cell (r,c) belongs to exactly 2 LTP sub-tables (B.57 full coverage).
 *
 * Iterates all 16,129 cells. Verifies ltpMembership(r,c).count == 2.
 */
TEST(LtpTableTest, AllCellsCoveredExactlyTwoTimes) {
    std::uint32_t covered = 0;
    for (std::uint16_t r = 0; r < kLtpS; ++r) {
        for (std::uint16_t c = 0; c < kLtpS; ++c) {
            const auto &mem = ltpMembership(r, c);
            EXPECT_EQ(mem.count, static_cast<std::uint8_t>(2))
                << "Cell (" << r << "," << c << ") has count " << static_cast<int>(mem.count)
                << " (expected 2)";
            ++covered;
        }
    }
    EXPECT_EQ(covered, kTotalCells);
}

// ---------------------------------------------------------------------------
// Forward/reverse consistency: ltpMembership agrees with ltp1..2CellsForLine
// ---------------------------------------------------------------------------

/**
 * @brief For sampled cells, each flat index from ltpMembership resolves to the correct reverse table.
 *
 * Checks that cell (r,c) appears in the reverse line identified by its flat index.
 */
TEST(LtpTableTest, ForwardReverseConsistency) {
    for (const std::uint16_t r : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(63),
                                   static_cast<std::uint16_t>(126)}) {
        for (const std::uint16_t c : {static_cast<std::uint16_t>(0),
                                       static_cast<std::uint16_t>(63),
                                       static_cast<std::uint16_t>(126)}) {
            const auto &mem = ltpMembership(r, c);
            for (std::uint8_t j = 0; j < mem.count; ++j) {
                const auto f = mem.flat[j]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                bool found = false;
                if (f < static_cast<std::uint16_t>(kLtp2Base)) {
                    const auto lineIdx = static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp1Base));
                    for (const auto &cell : ltp1CellsForLine(lineIdx)) {
                        if (cell.r == r && cell.c == c) { found = true; break; }
                    }
                    EXPECT_TRUE(found) << "Cell (" << r << "," << c << ") not in LTP1 line " << lineIdx;
                } else {
                    const auto lineIdx = static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp2Base));
                    for (const auto &cell : ltp2CellsForLine(lineIdx)) {
                        if (cell.r == r && cell.c == c) { found = true; break; }
                    }
                    EXPECT_TRUE(found) << "Cell (" << r << "," << c << ") not in LTP2 line " << lineIdx;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Seed reproducibility: ltpMembership is deterministic
// ---------------------------------------------------------------------------

/**
 * @brief ltpMembership is deterministic: calling it twice returns identical results.
 *
 * The underlying table is a function-local static; this confirms idempotent initialization.
 */
TEST(LtpTableTest, SeedReproducibility) {
    const auto &a1 = ltpMembership(0, 0);
    const auto &a2 = ltpMembership(0, 0);
    EXPECT_EQ(a1.count, a2.count);
    for (std::uint8_t j = 0; j < a1.count; ++j) {
        EXPECT_EQ(a1.flat[j], a2.flat[j]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }

    const auto &b1 = ltpMembership(63, 50);
    const auto &b2 = ltpMembership(63, 50);
    EXPECT_EQ(b1.count, b2.count);
    for (std::uint8_t j = 0; j < b1.count; ++j) {
        EXPECT_EQ(b1.flat[j], b2.flat[j]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }
}

// ---------------------------------------------------------------------------
// Flat index ranges are within valid bounds for each sub-table
// ---------------------------------------------------------------------------

/**
 * @brief All flat indices from ltpMembership fall within valid sub-table ranges.
 *
 * For sampled cells, flat indices must be in [kLtp1Base, kLtp2Base+127).
 */
TEST(LtpTableTest, FlatIndicesInBounds) {
    for (const std::uint16_t r : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(63),
                                   static_cast<std::uint16_t>(126)}) {
        for (const std::uint16_t c : {static_cast<std::uint16_t>(0),
                                       static_cast<std::uint16_t>(63),
                                       static_cast<std::uint16_t>(126)}) {
            const auto &mem = ltpMembership(r, c);
            for (std::uint8_t j = 0; j < mem.count; ++j) {
                const auto f = mem.flat[j]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                EXPECT_GE(f, static_cast<std::uint16_t>(kLtp1Base))
                    << "Cell (" << r << "," << c << ") flat[" << j << "] below kLtp1Base";
                EXPECT_LT(f, static_cast<std::uint16_t>(kLtp2Base + kLtpNumLines))
                    << "Cell (" << r << "," << c << ") flat[" << j << "] above LTP2 range";
            }
        }
    }
}
