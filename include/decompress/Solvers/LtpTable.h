/**
 * @file LtpTable.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Joint-tiled variable-length LTP partitions LTP1–LTP4 for B.21.
 *
 * Four joint-tiled sub-tables partitioning all s^2 = 261,121 cells into 511 lines of
 * variable length ltp_len(k) = min(k+1, 511-k) (1 to 256 cells). Each cell belongs to
 * 1 sub-table (260,098 cells) or 2 sub-tables (1,023 corner cells).
 * Forward table: cell → LtpMembership. Reverse table: line → span of LtpCell.
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
     * Each cell belongs to 1 sub-table (count=1) or 2 sub-tables (count=2).
     * flat[i] is the flat stat-array index (e.g. kLtp1Base + line_k) for sub-table i.
     */
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    struct LtpMembership {
        /**
         * @name count
         * @brief Number of LTP sub-tables this cell belongs to (1 or 2).
         */
        std::uint8_t count{0};

        /**
         * @name flat
         * @brief Flat stat-array indices; flat[1] unused when count==1.
         */
        std::array<std::uint16_t, 2> flat{};
    };
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    /**
     * @name ltpLineLen
     * @brief Compute the length of LTP line k: min(k+1, kLtpS-k).
     *
     * Triangular: line 0 has 1 cell, line 255 has 256 cells, line 510 has 1 cell.
     *
     * @param k Line index in [0, kLtpNumLines).
     * @return Number of cells on the line (1 to 256).
     */
    [[nodiscard]] inline std::uint16_t ltpLineLen(const std::uint16_t k) noexcept {
        const auto kp1 = static_cast<std::uint16_t>(k + 1U);
        const auto sMinusK = static_cast<std::uint16_t>(kLtpS - k);
        return (kp1 < sMinusK) ? kp1 : sMinusK;
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
