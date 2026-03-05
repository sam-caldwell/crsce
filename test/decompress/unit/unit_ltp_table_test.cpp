/**
 * @file unit_ltp_table_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Unit tests for LtpTable: B.21 joint-tiled variable-length partitions.
 *
 * B.21 invariants verified:
 *   Axiom 1 (Conservation): sum of all line lengths per sub-table = 65,536 = 2^16.
 *   Axiom 2 (No duplicates): each cell appears at most once per line.
 *   Axiom 3 (Variable lengths): ltp_len(k) = min(k+1, 511-k), k=0→1, k=255→256, k=510→1.
 *   Coverage: every cell (r,c) has ltpMembership count 1 or 2.
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
using crsce::decompress::solvers::kLtp3Base;
using crsce::decompress::solvers::kLtp4Base;
using crsce::decompress::solvers::kLtpNumLines;
using crsce::decompress::solvers::kLtpS;
using crsce::decompress::solvers::ltp1CellsForLine;
using crsce::decompress::solvers::ltp2CellsForLine;
using crsce::decompress::solvers::ltp3CellsForLine;
using crsce::decompress::solvers::ltp4CellsForLine;
using crsce::decompress::solvers::ltpLineLen;
using crsce::decompress::solvers::ltpMembership;

namespace {
    /// sum_{k=0}^{510} min(k+1, 511-k) = 2^16 = 65,536
    constexpr std::uint32_t kCellsPerSubTable = 65536U;
    constexpr std::uint32_t kTotalCells = static_cast<std::uint32_t>(kLtpS) * kLtpS; // 261,121
} // namespace

// ---------------------------------------------------------------------------
// Axiom 3: ltpLineLen returns correct values
// ---------------------------------------------------------------------------

/**
 * @brief ltpLineLen(k) = min(k+1, 511-k): check boundary and peak values.
 */
TEST(LtpTableTest, LtpLineLenBoundaryAndPeak) {
    EXPECT_EQ(ltpLineLen(0),   1);
    EXPECT_EQ(ltpLineLen(1),   2);
    EXPECT_EQ(ltpLineLen(254), 255);
    EXPECT_EQ(ltpLineLen(255), 256);
    EXPECT_EQ(ltpLineLen(256), 255);
    EXPECT_EQ(ltpLineLen(509), 2);
    EXPECT_EQ(ltpLineLen(510), 1);
}

// ---------------------------------------------------------------------------
// LTP1 Axiom 1: sum of all line lengths = kCellsPerSubTable
// ---------------------------------------------------------------------------

/**
 * @brief Axiom 1 for LTP1: total cells across all 511 LTP1 lines = 65,536.
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
 * Samples lines 0, 255, and 510 (first, peak, last) for speed.
 */
TEST(LtpTableTest, Ltp1AxiomTwoNoDuplicatesInLine) {
    for (const std::uint16_t k : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(255),
                                   static_cast<std::uint16_t>(510)}) {
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
 * @brief Axiom 1 for LTP2: total cells across all 511 LTP2 lines = 65,536.
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
                                   static_cast<std::uint16_t>(255),
                                   static_cast<std::uint16_t>(510)}) {
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

// ---------------------------------------------------------------------------
// LTP3 Axiom 1: sum of all line lengths = kCellsPerSubTable
// ---------------------------------------------------------------------------

/**
 * @brief Axiom 1 for LTP3: total cells across all 511 LTP3 lines = 65,536.
 */
TEST(LtpTableTest, Ltp3AxiomOneConservation) {
    std::uint32_t count = 0;
    for (std::uint16_t k = 0; k < kLtpNumLines; ++k) {
        const auto cells = ltp3CellsForLine(k);
        count += static_cast<std::uint32_t>(cells.size());
    }
    EXPECT_EQ(count, kCellsPerSubTable);
}

/**
 * @brief Axiom 2 for LTP3: each sampled line has exactly ltpLineLen(k) distinct cells.
 */
TEST(LtpTableTest, Ltp3AxiomTwoNoDuplicatesInLine) {
    for (const std::uint16_t k : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(255),
                                   static_cast<std::uint16_t>(510)}) {
        const auto cells = ltp3CellsForLine(k);
        EXPECT_EQ(cells.size(), static_cast<std::size_t>(ltpLineLen(k)));
        std::set<std::pair<std::uint16_t, std::uint16_t>> seen;
        for (const auto &cell : cells) {
            const auto [it, inserted] = seen.emplace(cell.r, cell.c);
            EXPECT_TRUE(inserted) << "Duplicate cell (" << cell.r << "," << cell.c
                                  << ") in LTP3 line " << k;
        }
    }
}

// ---------------------------------------------------------------------------
// LTP4 Axiom 1: sum of all line lengths = kCellsPerSubTable
// ---------------------------------------------------------------------------

/**
 * @brief Axiom 1 for LTP4: total cells across all 511 LTP4 lines = 65,536.
 */
TEST(LtpTableTest, Ltp4AxiomOneConservation) {
    std::uint32_t count = 0;
    for (std::uint16_t k = 0; k < kLtpNumLines; ++k) {
        const auto cells = ltp4CellsForLine(k);
        count += static_cast<std::uint32_t>(cells.size());
    }
    EXPECT_EQ(count, kCellsPerSubTable);
}

/**
 * @brief Axiom 2 for LTP4: each sampled line has exactly ltpLineLen(k) distinct cells.
 */
TEST(LtpTableTest, Ltp4AxiomTwoNoDuplicatesInLine) {
    for (const std::uint16_t k : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(255),
                                   static_cast<std::uint16_t>(510)}) {
        const auto cells = ltp4CellsForLine(k);
        EXPECT_EQ(cells.size(), static_cast<std::size_t>(ltpLineLen(k)));
        std::set<std::pair<std::uint16_t, std::uint16_t>> seen;
        for (const auto &cell : cells) {
            const auto [it, inserted] = seen.emplace(cell.r, cell.c);
            EXPECT_TRUE(inserted) << "Duplicate cell (" << cell.r << "," << cell.c
                                  << ") in LTP4 line " << k;
        }
    }
}

// ---------------------------------------------------------------------------
// Coverage: every cell has membership count 1 or 2
// ---------------------------------------------------------------------------

/**
 * @brief Every cell (r,c) belongs to exactly 1 or 2 LTP sub-tables (B.21 joint tiling).
 *
 * Iterates all 261,121 cells. Verifies ltpMembership(r,c).count in {1,2}.
 */
TEST(LtpTableTest, AllCellsCoveredOnceOrTwice) {
    std::uint32_t covered = 0;
    for (std::uint16_t r = 0; r < kLtpS; ++r) {
        for (std::uint16_t c = 0; c < kLtpS; ++c) {
            const auto &mem = ltpMembership(r, c);
            EXPECT_GE(mem.count, static_cast<std::uint8_t>(1))
                << "Cell (" << r << "," << c << ") not in any sub-table";
            EXPECT_LE(mem.count, static_cast<std::uint8_t>(2))
                << "Cell (" << r << "," << c << ") in more than 2 sub-tables";
            ++covered;
        }
    }
    EXPECT_EQ(covered, kTotalCells);
}

// ---------------------------------------------------------------------------
// Forward/reverse consistency: ltpMembership agrees with ltp1..4CellsForLine
// ---------------------------------------------------------------------------

/**
 * @brief For sampled cells, each flat index from ltpMembership resolves to the correct reverse table.
 *
 * Checks that cell (r,c) appears in the reverse line identified by its flat index.
 */
TEST(LtpTableTest, ForwardReverseConsistency) {
    for (const std::uint16_t r : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(100),
                                   static_cast<std::uint16_t>(255),
                                   static_cast<std::uint16_t>(510)}) {
        for (const std::uint16_t c : {static_cast<std::uint16_t>(0),
                                       static_cast<std::uint16_t>(100),
                                       static_cast<std::uint16_t>(255),
                                       static_cast<std::uint16_t>(510)}) {
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
                } else if (f < static_cast<std::uint16_t>(kLtp3Base)) {
                    const auto lineIdx = static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp2Base));
                    for (const auto &cell : ltp2CellsForLine(lineIdx)) {
                        if (cell.r == r && cell.c == c) { found = true; break; }
                    }
                    EXPECT_TRUE(found) << "Cell (" << r << "," << c << ") not in LTP2 line " << lineIdx;
                } else if (f < static_cast<std::uint16_t>(kLtp4Base)) {
                    const auto lineIdx = static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp3Base));
                    for (const auto &cell : ltp3CellsForLine(lineIdx)) {
                        if (cell.r == r && cell.c == c) { found = true; break; }
                    }
                    EXPECT_TRUE(found) << "Cell (" << r << "," << c << ") not in LTP3 line " << lineIdx;
                } else {
                    const auto lineIdx = static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(kLtp4Base));
                    for (const auto &cell : ltp4CellsForLine(lineIdx)) {
                        if (cell.r == r && cell.c == c) { found = true; break; }
                    }
                    EXPECT_TRUE(found) << "Cell (" << r << "," << c << ") not in LTP4 line " << lineIdx;
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

    const auto &b1 = ltpMembership(255, 100);
    const auto &b2 = ltpMembership(255, 100);
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
 * For sampled cells, flat indices must be in [kLtp1Base, kLtp4Base+511).
 */
TEST(LtpTableTest, FlatIndicesInBounds) {
    for (const std::uint16_t r : {static_cast<std::uint16_t>(0),
                                   static_cast<std::uint16_t>(255),
                                   static_cast<std::uint16_t>(510)}) {
        for (const std::uint16_t c : {static_cast<std::uint16_t>(0),
                                       static_cast<std::uint16_t>(255),
                                       static_cast<std::uint16_t>(510)}) {
            const auto &mem = ltpMembership(r, c);
            for (std::uint8_t j = 0; j < mem.count; ++j) {
                const auto f = mem.flat[j]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                EXPECT_GE(f, static_cast<std::uint16_t>(kLtp1Base))
                    << "Cell (" << r << "," << c << ") flat[" << j << "] below kLtp1Base";
                EXPECT_LT(f, static_cast<std::uint16_t>(kLtp4Base + kLtpNumLines))
                    << "Cell (" << r << "," << c << ") flat[" << j << "] above LTP4 range";
            }
        }
    }
}
