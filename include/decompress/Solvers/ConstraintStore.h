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
     * for all 6s-2 + 4s = 10s-2 lines (s rows, s columns, 2s-1 diagonals,
     * 2s-1 anti-diagonals, and 4 toroidal-slope partitions of s lines each).
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
         * @name kNumSlope
         * @brief Number of lines per toroidal-slope partition (s).
         */
        static constexpr std::uint16_t kNumSlope = kS;

        /**
         * @name kNumSlopePartitions
         * @brief Number of toroidal-slope partitions (4).
         */
        static constexpr std::uint16_t kNumSlopePartitions = 4;

        /**
         * @name kBasicLines
         * @brief Number of original constraint lines: 6s - 2 = 3064.
         */
        static constexpr std::uint32_t kBasicLines = kNumRows + kNumCols + kNumDiags + kNumAntiDiags;

        /**
         * @name kTotalLines
         * @brief Total number of constraint lines: 6s - 2 + 4s = 10s - 2 = 5108.
         */
        static constexpr std::uint32_t kTotalLines = kBasicLines + (kNumSlopePartitions * kNumSlope);

        /**
         * @name kSlopes
         * @brief Slope values for the four toroidal partitions.
         */
        static constexpr std::array<std::uint16_t, kNumSlopePartitions> kSlopes = {256, 255, 2, 509};

        /**
         * @name kSlope256Base
         * @brief Flat index base for the slope-256 (HSM1) partition.
         */
        static constexpr std::uint32_t kSlope256Base = kBasicLines;

        /**
         * @name kSlope255Base
         * @brief Flat index base for the slope-255 (SFC1) partition.
         */
        static constexpr std::uint32_t kSlope255Base = kSlope256Base + kNumSlope;

        /**
         * @name kSlope2Base
         * @brief Flat index base for the slope-2 (HSM2) partition.
         */
        static constexpr std::uint32_t kSlope2Base = kSlope255Base + kNumSlope;

        /**
         * @name kSlope509Base
         * @brief Flat index base for the slope-509 (SFC2) partition.
         */
        static constexpr std::uint32_t kSlope509Base = kSlope2Base + kNumSlope;

        /**
         * @name kSlopeBases
         * @brief Flat index base offsets for each slope partition, indexed by partition ordinal.
         */
        static constexpr std::array<std::uint32_t, kNumSlopePartitions> kSlopeBases = {
            kSlope256Base, kSlope255Base, kSlope2Base, kSlope509Base
        };

        /**
         * @name lineIndex
         * @brief Map a LineID to a flat index in [0, kTotalLines).
         *
         * Layout: rows [0, kS), cols [kS, 2*kS), diags [2*kS, 2*kS + kNumDiags),
         * anti-diags [2*kS + kNumDiags, kTotalLines).
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
                case LineType::Slope256:     return kSlope256Base + line.index;
                case LineType::Slope255:     return kSlope255Base + line.index;
                case LineType::Slope2:       return kSlope2Base + line.index;
                case LineType::Slope509:     return kSlope509Base + line.index;
            }
            return 0; // unreachable
        }

        /**
         * @name ConstraintStore
         * @brief Construct a constraint store with the given target sums.
         * @param rowSums Target row sums (LSM), size s.
         * @param colSums Target column sums (VSM), size s.
         * @param diagSums Target diagonal sums (DSM), size 2s-1.
         * @param antiDiagSums Target anti-diagonal sums (XSM), size 2s-1.
         * @param slope256Sums Target slope-256 (HSM1) sums, size s.
         * @param slope255Sums Target slope-255 (SFC1) sums, size s.
         * @param slope2Sums Target slope-2 (HSM2) sums, size s.
         * @param slope509Sums Target slope-509 (SFC2) sums, size s.
         * @throws None
         */
        ConstraintStore(const std::vector<std::uint16_t> &rowSums,
                        const std::vector<std::uint16_t> &colSums,
                        const std::vector<std::uint16_t> &diagSums,
                        const std::vector<std::uint16_t> &antiDiagSums,
                        const std::vector<std::uint16_t> &slope256Sums,
                        const std::vector<std::uint16_t> &slope255Sums,
                        const std::vector<std::uint16_t> &slope2Sums,
                        const std::vector<std::uint16_t> &slope509Sums);

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

        /**
         * @name slopeFlatIndices
         * @brief Get precomputed flat stat indices for the 4 slope partitions at cell (r, c).
         *
         * Returns a reference to 4 precomputed uint16_t flat stat indices, eliminating
         * the expensive modular arithmetic (% 511) from hot paths. The table is computed
         * once on first access and shared across all ConstraintStore instances.
         *
         * @param r Row index in [0, kS).
         * @param c Column index in [0, kS).
         * @return Reference to array of 4 flat stat indices [slope256, slope255, slope2, slope509].
         */
        [[nodiscard]] static const std::array<std::uint16_t, kNumSlopePartitions> &
            slopeFlatIndices(std::uint16_t r, std::uint16_t c);

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
         * @brief Unified per-line statistics for all 6s-2 lines.
         *
         * Layout: rows [0,kS), cols [kS,2*kS), diags [2*kS, 2*kS+kNumDiags),
         * anti-diags [2*kS+kNumDiags, kTotalLines).
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
