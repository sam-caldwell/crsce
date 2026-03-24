/**
 * @file LtpTable.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Full-coverage uniform-511 LTP partitions LTP1–LTP6 (B.25/B.27).
 *
 * Six independent sub-tables, each covering all 261,121 cells exactly once.
 * Every line has exactly 511 cells (ltp_len(k) = kLtpS for all k).
 * Sum per sub-table = 511 * 511 = 261,121.
 * Each cell belongs to exactly one line in each of the six sub-tables (count always 6).
 * Sub-tables use independent LCG shuffles so each line spans a diverse cross-section of rows.
 * Forward table: cell → LtpMembership (count=6, flat[0..5]).
 * Reverse table: line → span of kLtpS LtpCell entries.
 *
 * B.22 seed search: seeds are runtime-overridable via CRSCE_LTP_SEED_1..6 env vars.
 * B.23 clipped-triangular experiment (TESTED, ABANDONED): partial coverage caused severe
 * regression (~46K depth vs ~86K for uniform-511); see B.23 section in spec.
 * B.27: added LTP5 and LTP6 (seeds CRSCLTP5/CRSCLTP6) to increase constraint density to 10
 * lines per cell (6 LTP + 4 basic); wire format expanded to 16,899 bytes per block.
 */
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace crsce::decompress::solvers {

    /**
     * @name kLtpS
     * @brief Matrix dimension (511).
     */
    inline constexpr std::uint16_t kLtpS = 127;

    /**
     * @name kLtpNumLines
     * @brief Lines per LTP partition (511).
     */
    inline constexpr std::uint16_t kLtpNumLines = 127;

    /**
     * @name kLtp1Base
     * @brief Flat stat-array base for LTP1 lines (= kBasicLines = 6s-2 = 3064).
     */
    // B.57: kBasicLines = s + s + (2s-1) + (2s-1) = 6s-2 = 760 for S=127
    inline constexpr std::uint32_t kLtp1Base = 760;

    /**
     * @name kLtp2Base
     * @brief Flat stat-array base for LTP2 lines (kLtp1Base + s = 887).
     */
    inline constexpr std::uint32_t kLtp2Base = kLtp1Base + kLtpS; // 887

    /**
     * @name kLtp3Base
     * @brief Flat stat-array base for LTP3 lines (unused in B.57, 2-LTP config).
     */
    inline constexpr std::uint32_t kLtp3Base = kLtp2Base + kLtpS; // 1014

    /**
     * @name kLtp4Base
     * @brief Flat stat-array base for LTP4 lines (unused in B.57).
     */
    inline constexpr std::uint32_t kLtp4Base = kLtp3Base + kLtpS; // 1141

    /**
     * @name kLtp5Base
     * @brief Flat stat-array base for LTP5 lines (unused in B.57).
     */
    inline constexpr std::uint32_t kLtp5Base = kLtp4Base + kLtpS; // 1268

    /**
     * @name kLtpNumVarlenLines
     * @brief Lines per variable-length rLTP partition (2s-1 = 1021, B.46).
     */
    inline constexpr std::uint16_t kLtpNumVarlenLines = (2 * kLtpS) - 1;

    /**
     * @name kLtp6Base
     * @brief Flat stat-array base for LTP6 lines. B.46: offset by 1021 (rLTP5 capacity).
     */
    inline constexpr std::uint32_t kLtp6Base = kLtp5Base + kLtpNumVarlenLines; // 6129

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
     * Under B.27 full-coverage: every cell belongs to exactly one line in each of the six
     * sub-tables (count always 6).  flat[i] is the flat stat-array index for sub-table i.
     */
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    struct LtpMembership {
        /**
         * @name count
         * @brief Number of LTP sub-tables this cell belongs to (always 6 under B.27).
         */
        std::uint8_t count{0};

        /**
         * @name flat
         * @brief Flat stat-array indices for LTP sub-tables 0..5.
         */
        std::array<std::uint16_t, 6> flat{};
    };
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    /**
     * @name ltpLineLen
     * @brief Return the length of LTP line k: always kLtpS (511) under B.25.
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
     * @name ltpVarLineLen
     * @brief Return the length of variable-length rLTP line k (B.46).
     *
     * Follows the diagonal/anti-diagonal pattern: min(k+1, 511, 1021-k).
     * Line 0 has 1 cell, line 510 has 511 cells, line 1020 has 1 cell.
     *
     * @param k Line index in [0, kLtpNumVarlenLines).
     * @return Length of line k.
     */
    [[nodiscard]] inline std::uint32_t ltpVarLineLen(const std::uint16_t k) noexcept {
        const auto kp1 = static_cast<std::uint32_t>(k) + 1U;
        const auto s = static_cast<std::uint32_t>(kLtpS);
        const auto mirror = static_cast<std::uint32_t>(kLtpNumVarlenLines) - static_cast<std::uint32_t>(k);
        return std::min({kp1, s, mirror});
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

    /**
     * @name ltp5CellsForLine
     * @brief Return the cells on LTP5 line k.
     * @param k Line index in [0, kLtpNumLines).
     * @return Span of ltpLineLen(k) LtpCell entries.
     */
    [[nodiscard]] std::span<const LtpCell> ltp5CellsForLine(std::uint16_t k);

    /**
     * @name ltp6CellsForLine
     * @brief Return the cells on LTP6 line k.
     * @param k Line index in [0, kLtpNumLines).
     * @return Span of ltpLineLen(k) LtpCell entries.
     */
    [[nodiscard]] std::span<const LtpCell> ltp6CellsForLine(std::uint16_t k);

    /**
     * @name ltpFileIsValid
     * @brief Return true iff the file at path can be parsed as a valid LTPB LTP table.
     *
     * Does NOT cache or modify the shared LTP singleton.  Used by tests and
     * health-check tooling to verify file integrity independently of getLtpData().
     *
     * @param path Filesystem path to an LTPB file.
     * @return True if the file parses successfully; false on any error.
     */
    [[nodiscard]] bool ltpFileIsValid(const char *path) noexcept;

} // namespace crsce::decompress::solvers
