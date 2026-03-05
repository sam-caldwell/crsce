/**
 * @file ConstraintStore.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Concrete constraint store managing cell assignments and per-line statistics.
 */
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/IConstraintStore.h"
#include "decompress/Solvers/LineID.h"

namespace crsce::decompress::solvers {
    /**
     * @class ConstraintStore
     * @name ConstraintStore
     * @brief Manages cell assignments and per-line statistics (u, a, rho) for the solver.
     *
     * Tracks the assignment state of all s^2 cells and maintains per-line statistics
     * for all 10s-2 lines (s rows, s columns, 2s-1 diagonals, 2s-1 anti-diagonals,
     * and 4 pseudorandom LTP partitions of s lines each).
     *
     * B.20: replaced 4 toroidal-slope partitions with 4 LTP pseudorandom partitions.
     * Total lines: 10s-2 = 5108 (unchanged from pre-B.9 slope layout).
     */
    class ConstraintStore final : public IConstraintStore {
    public:
        /**
         * @name kS
         * @brief Matrix dimension (511).
         */
        static constexpr std::uint16_t kS = 511;

        /**
         * @name kNumRows
         * @brief Number of row constraints.
         */
        static constexpr std::uint16_t kNumRows = kS;

        /**
         * @name kNumCols
         * @brief Number of column constraints.
         */
        static constexpr std::uint16_t kNumCols = kS;

        /**
         * @name kNumDiags
         * @brief Number of diagonal constraints (2s - 1).
         */
        static constexpr std::uint16_t kNumDiags = (2 * kS) - 1;

        /**
         * @name kNumAntiDiags
         * @brief Number of anti-diagonal constraints (2s - 1).
         */
        static constexpr std::uint16_t kNumAntiDiags = (2 * kS) - 1;

        /**
         * @name kNumLtpPartitions
         * @brief Number of non-linear lookup-table partitions (4: LTP1–LTP4).
         */
        static constexpr std::uint16_t kNumLtpPartitions = 4;

        /**
         * @name kBasicLines
         * @brief Number of original constraint lines: 6s - 2 = 3064.
         */
        static constexpr std::uint32_t kBasicLines = kNumRows + kNumCols + kNumDiags + kNumAntiDiags;

        /**
         * @name kLTP1Base
         * @brief Flat index base for the LTP1 non-linear partition (= kBasicLines = 3064).
         */
        static constexpr std::uint32_t kLTP1Base = kBasicLines;

        /**
         * @name kLTP2Base
         * @brief Flat index base for the LTP2 non-linear partition (7s-2 = 3575).
         */
        static constexpr std::uint32_t kLTP2Base = kLTP1Base + kS;

        /**
         * @name kLTP3Base
         * @brief Flat index base for the LTP3 non-linear partition (8s-2 = 4086).
         */
        static constexpr std::uint32_t kLTP3Base = kLTP2Base + kS;

        /**
         * @name kLTP4Base
         * @brief Flat index base for the LTP4 non-linear partition (9s-2 = 4597).
         */
        static constexpr std::uint32_t kLTP4Base = kLTP3Base + kS;

        /**
         * @name kTotalLines
         * @brief Total number of constraint lines: 10s - 2 = 5108.
         */
        static constexpr std::uint32_t kTotalLines = kBasicLines + (kNumLtpPartitions * kS);

        /**
         * @name lineIndex
         * @brief Map a LineID to a flat index in [0, kTotalLines).
         *
         * Layout: rows [0, kS), cols [kS, 2*kS), diags [2*kS, 2*kS + kNumDiags),
         * anti-diags [2*kS + kNumDiags, kBasicLines), LTP1–LTP4 partitions follow.
         *
         * @param line The line identifier.
         * @return Flat index into the unified stats array.
         */
        [[nodiscard]] static constexpr std::uint32_t lineIndex(const LineID line) {
            switch (line.type) {
                case LineType::Row:          return line.index;
                case LineType::Column:       return kS + line.index;
                case LineType::Diagonal:     return (2U * kS) + line.index;
                case LineType::AntiDiagonal: return (2U * kS) + kNumDiags + line.index;
                case LineType::LTP1:         return kLTP1Base + line.index;
                case LineType::LTP2:         return kLTP2Base + line.index;
                case LineType::LTP3:         return kLTP3Base + line.index;
                case LineType::LTP4:         return kLTP4Base + line.index;
            }
            return 0; // unreachable
        }

        /**
         * @name flatIndexToLineID
         * @brief Inverse of lineIndex: map a flat index back to a LineID.
         *
         * Layout (same as lineIndex):
         *   rows       [0,         kNumRows)
         *   cols       [kNumRows,  kNumRows + kNumCols)
         *   diags      [kNumRows + kNumCols, kNumRows + kNumCols + kNumDiags)
         *   anti-diags [kNumRows + kNumCols + kNumDiags, kBasicLines)
         *   ltp1       [kLTP1Base, kLTP2Base)
         *   ltp2       [kLTP2Base, kLTP3Base)
         *   ltp3       [kLTP3Base, kLTP4Base)
         *   ltp4       [kLTP4Base, kTotalLines)
         *
         * @param idx Flat index in [0, kTotalLines).
         * @return The corresponding LineID.
         */
        [[nodiscard]] static constexpr LineID flatIndexToLineID(const std::uint32_t idx) noexcept {
            if (idx < kNumRows) {
                return {.type = LineType::Row,
                        .index = static_cast<std::uint16_t>(idx)};
            }
            if (idx < kNumRows + kNumCols) {
                return {.type = LineType::Column,
                        .index = static_cast<std::uint16_t>(idx - kNumRows)};
            }
            if (idx < kNumRows + kNumCols + kNumDiags) {
                return {.type = LineType::Diagonal,
                        .index = static_cast<std::uint16_t>(idx - kNumRows - kNumCols)};
            }
            if (idx < kBasicLines) {
                return {.type = LineType::AntiDiagonal,
                        .index = static_cast<std::uint16_t>(
                            idx - kNumRows - kNumCols - kNumDiags)};
            }
            if (idx < kLTP2Base) {
                return {.type = LineType::LTP1,
                        .index = static_cast<std::uint16_t>(idx - kLTP1Base)};
            }
            if (idx < kLTP3Base) {
                return {.type = LineType::LTP2,
                        .index = static_cast<std::uint16_t>(idx - kLTP2Base)};
            }
            if (idx < kLTP4Base) {
                return {.type = LineType::LTP3,
                        .index = static_cast<std::uint16_t>(idx - kLTP3Base)};
            }
            return {.type = LineType::LTP4,
                    .index = static_cast<std::uint16_t>(idx - kLTP4Base)};
        }

        /**
         * @name ConstraintStore
         * @brief Construct a constraint store with the given target sums.
         * @param rowSums Target row sums (LSM), size s.
         * @param colSums Target column sums (VSM), size s.
         * @param diagSums Target diagonal sums (DSM), size 2s-1.
         * @param antiDiagSums Target anti-diagonal sums (XSM), size 2s-1.
         * @param ltp1Sums Target LTP1 partition sums, size s.
         * @param ltp2Sums Target LTP2 partition sums, size s.
         * @param ltp3Sums Target LTP3 partition sums, size s.
         * @param ltp4Sums Target LTP4 partition sums, size s.
         * @throws None
         */
        ConstraintStore(const std::vector<std::uint16_t> &rowSums,
                        const std::vector<std::uint16_t> &colSums,
                        const std::vector<std::uint16_t> &diagSums,
                        const std::vector<std::uint16_t> &antiDiagSums,
                        const std::vector<std::uint16_t> &ltp1Sums,
                        const std::vector<std::uint16_t> &ltp2Sums,
                        const std::vector<std::uint16_t> &ltp3Sums,
                        const std::vector<std::uint16_t> &ltp4Sums);

        void assign(std::uint16_t r, std::uint16_t c, std::uint8_t v) override;
        void unassign(std::uint16_t r, std::uint16_t c) override;
        [[nodiscard]] std::int32_t getResidual(LineID line) const override;
        [[nodiscard]] std::uint16_t getUnknownCount(LineID line) const override;
        [[nodiscard]] std::uint16_t getAssignedCount(LineID line) const override;
        [[nodiscard]] std::array<LineID, 8> getLinesForCell(std::uint16_t r, std::uint16_t c) const override;
        [[nodiscard]] CellState getCellState(std::uint16_t r, std::uint16_t c) const override;
        [[nodiscard]] std::uint8_t getCellValue(std::uint16_t r, std::uint16_t c) const override;
        [[nodiscard]] std::uint16_t getRowUnknownCount(std::uint16_t r) const override;
        [[nodiscard]] const std::array<std::uint64_t, 8> &getRow(std::uint16_t r) const override;

        /**
         * @name getFirstUnassigned
         * @brief Find the first unassigned cell at or after startRow using bitwise scanning.
         * @param startRow Row index to begin scanning from.
         * @return Pair (r, c) of the first unassigned cell, or nullopt if all assigned.
         */
        [[nodiscard]] std::optional<std::pair<std::uint16_t, std::uint16_t>>
            getFirstUnassigned(std::uint16_t startRow) const;

        /**
         * @struct LineStat
         * @name LineStat
         * @brief Per-line statistics: target sum S, unknown count u, assigned-ones count a.
         */
        struct LineStat {
            /**
             * @name target
             * @brief The target sum S(L) from the cross-sum vector.
             */
            std::uint16_t target{0};

            /**
             * @name unknown
             * @brief Count of unassigned cells u(L).
             */
            std::uint16_t unknown{0};

            /**
             * @name assigned
             * @brief Count of assigned-one cells a(L).
             */
            std::uint16_t assigned{0};
        };

        /**
         * @name getStatDirect
         * @brief Direct access to per-line statistics by flat index (no virtual dispatch).
         * @param idx Flat index in [0, kTotalLines).
         * @return Const reference to the LineStat at the given index.
         */
        [[nodiscard]] const LineStat &getStatDirect(std::uint32_t idx) const {
            return stats_[idx]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }

    private:

        /**
         * @name lineLen
         * @brief Compute the length of a line (number of cells).
         * @param line The line identifier.
         * @return Number of cells on the line.
         * @throws None
         */
        [[nodiscard]] std::uint16_t lineLen(LineID line) const;

        /**
         * @name cells_
         * @brief Assignment state of each cell (flat, row-major: cells_[r * kS + c]).
         */
        std::vector<CellState> cells_;

        /**
         * @name stats_
         * @brief Unified per-line statistics for all 10s-2 lines.
         *
         * Layout: rows [0,kS), cols [kS,2*kS), diags [2*kS, 2*kS+kNumDiags),
         * anti-diags [2*kS+kNumDiags, kBasicLines), LTP1–LTP4 follow.
         */
        std::array<LineStat, kTotalLines> stats_{};

        /**
         * @name rowBits_
         * @brief Row bit storage for hash verification (8 x uint64 per row, MSB-first).
         */
        std::vector<std::array<std::uint64_t, 8>> rowBits_;

        /**
         * @name assigned_
         * @brief Compact bitset tracking assigned cells (1 = assigned, 0 = unassigned).
         *
         * Same 511 x 8 x uint64 layout as rowBits_. Bit c in row r is at:
         *   assigned_[r][c / 64] & (1ULL << (c % 64))
         * Note: uses LSB-first bit addressing (unlike MSB-first rowBits_) for
         * efficient ctzll scanning in getFirstUnassigned().
         */
        std::array<std::array<std::uint64_t, 8>, kS> assigned_{};
    };
} // namespace crsce::decompress::solvers
