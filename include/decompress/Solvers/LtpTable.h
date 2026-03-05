/**
 * @file LtpTable.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Full-coverage uniform-511 LTP partitions LTP1–LTP4 for B.23.
 *
 * Four independent sub-tables, each assigning all 261,121 cells to exactly one of 511
 * uniform-length lines.  Every line has exactly 511 cells (ltp_len(k) = kLtpS for all k);
 * total per sub-table = 511 * 511 = 261,121.
 * Each cell belongs to exactly one line in each of the four sub-tables (count always 4).
 * Sub-tables use independent LCG shuffles so each line spans a diverse cross-section of rows.
 * Forward table: cell → LtpMembership (count=4, flat[0..3]).
 * Reverse table: line → span of LtpCell.
 *
 * B.23 hypothesis: uniform-511 lines (same structure as B.20) combined with the CDCL
 * backjumping infrastructure (B.1) should isolate the effect of CDCL on a known-good
 * partition.  Compare to B.20 (88,503 depth, no CDCL) and B.22 (80,300 depth, CDCL,
 * triangular 2–1021 distribution).
 */
#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace crsce::decompress::solvers {

    /**
     * @name kLtpS
     * @brief Matrix dimension (511).
     */
    inline constexpr std::uint16_t kLtpS = 511;

    /**
     * @name kLtpNumLines
     * @brief Lines per LTP partition (511).
     */
    inline constexpr std::uint16_t kLtpNumLines = 511;

    /**
     * @name kLtp1Base
     * @brief Flat stat-array base for LTP1 lines (= kBasicLines = 6s-2 = 3064).
     */
    inline constexpr std::uint32_t kLtp1Base = 3064;

    /**
     * @name kLtp2Base
     * @brief Flat stat-array base for LTP2 lines (7s-2 = 3575).
     */
    inline constexpr std::uint32_t kLtp2Base = 3575;

    /**
     * @name kLtp3Base
     * @brief Flat stat-array base for LTP3 lines (8s-2 = 4086).
     */
    inline constexpr std::uint32_t kLtp3Base = 4086;

    /**
     * @name kLtp4Base
     * @brief Flat stat-array base for LTP4 lines (9s-2 = 4597).
     */
    inline constexpr std::uint32_t kLtp4Base = 4597;

    /**
     * @struct LtpCell
     * @name LtpCell
     * @brief A (row, column) cell coordinate pair used in LTP reverse tables.
     */
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    struct LtpCell {
        /**
         * @name r
         * @brief Row index.
         */
        std::uint16_t r;

        /**
         * @name c
         * @brief Column index.
         */
        std::uint16_t c;
    };
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    /**
     * @struct LtpMembership
     * @name LtpMembership
     * @brief Forward-table entry: which LTP sub-table lines does cell (r,c) belong to?
     *
     * Under B.22 full-coverage: every cell belongs to exactly one line in each of the four
     * sub-tables (count always 4).  flat[i] is the flat stat-array index for sub-table i.
     */
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    struct LtpMembership {
        /**
         * @name count
         * @brief Number of LTP sub-tables this cell belongs to (always 4 under B.22).
         */
        std::uint8_t count{0};

        /**
         * @name flat
         * @brief Flat stat-array indices for LTP sub-tables 0..3.
         */
        std::array<std::uint16_t, 4> flat{};
    };
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    /**
     * @name ltpLineLen
     * @brief Return the length of LTP line k under B.23: always kLtpS (511).
     *
     * All 511 lines are uniform-length.  Sum over k=0..510 equals kLtpS*kLtpS = 261,121
     * (full matrix coverage).
     *
     * @param k Line index in [0, kLtpNumLines).  Unused (all lines same length).
     * @return kLtpS (511).
     */
    [[nodiscard]] inline std::uint32_t ltpLineLen([[maybe_unused]] const std::uint16_t k) noexcept {
        return static_cast<std::uint32_t>(kLtpS);
    }

    /**
     * @name ltpMembership
     * @brief Return precomputed LTP sub-table membership for cell (r, c).
     *
     * Returns which LTP sub-table(s) cell (r,c) belongs to and their flat stat indices.
     * Table is a function-local static, initialized on first call via B.21 construction.
     *
     * @param r Row index in [0, kLtpS).
     * @param c Column index in [0, kLtpS).
     * @return Const reference to LtpMembership for this cell.
     */
    [[nodiscard]] const LtpMembership &ltpMembership(std::uint16_t r, std::uint16_t c);

    /**
     * @name ltp1CellsForLine
     * @brief Return the cells on LTP1 line k.
     * @param k Line index in [0, kLtpNumLines).
     * @return Span of ltpLineLen(k) LtpCell entries.
     */
    [[nodiscard]] std::span<const LtpCell> ltp1CellsForLine(std::uint16_t k);

    /**
     * @name ltp2CellsForLine
     * @brief Return the cells on LTP2 line k.
     * @param k Line index in [0, kLtpNumLines).
     * @return Span of ltpLineLen(k) LtpCell entries.
     */
    [[nodiscard]] std::span<const LtpCell> ltp2CellsForLine(std::uint16_t k);

    /**
     * @name ltp3CellsForLine
     * @brief Return the cells on LTP3 line k.
     * @param k Line index in [0, kLtpNumLines).
     * @return Span of ltpLineLen(k) LtpCell entries.
     */
    [[nodiscard]] std::span<const LtpCell> ltp3CellsForLine(std::uint16_t k);

    /**
     * @name ltp4CellsForLine
     * @brief Return the cells on LTP4 line k.
     * @param k Line index in [0, kLtpNumLines).
     * @return Span of ltpLineLen(k) LtpCell entries.
     */
    [[nodiscard]] std::span<const LtpCell> ltp4CellsForLine(std::uint16_t k);

} // namespace crsce::decompress::solvers
