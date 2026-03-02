/**
 * @file ConstraintStore.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Concrete constraint store managing cell assignments and per-line statistics.
 */
#pragma once

#include <array>
#include <cstdint>
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
     * for all 6s-2 lines (s rows, s columns, 2s-1 diagonals, 2s-1 anti-diagonals).
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
         * @name ConstraintStore
         * @brief Construct a constraint store with the given target sums.
         * @param rowSums Target row sums (LSM), size s.
         * @param colSums Target column sums (VSM), size s.
         * @param diagSums Target diagonal sums (DSM), size 2s-1.
         * @param antiDiagSums Target anti-diagonal sums (XSM), size 2s-1.
         * @throws None
         */
        ConstraintStore(const std::vector<std::uint16_t> &rowSums,
                        const std::vector<std::uint16_t> &colSums,
                        const std::vector<std::uint16_t> &diagSums,
                        const std::vector<std::uint16_t> &antiDiagSums);

        void assign(std::uint16_t r, std::uint16_t c, std::uint8_t v) override;
        void unassign(std::uint16_t r, std::uint16_t c) override;
        [[nodiscard]] std::int32_t getResidual(LineID line) const override;
        [[nodiscard]] std::uint16_t getUnknownCount(LineID line) const override;
        [[nodiscard]] std::uint16_t getAssignedCount(LineID line) const override;
        [[nodiscard]] std::array<LineID, 4> getLinesForCell(std::uint16_t r, std::uint16_t c) const override;
        [[nodiscard]] CellState getCellState(std::uint16_t r, std::uint16_t c) const override;
        [[nodiscard]] std::uint8_t getCellValue(std::uint16_t r, std::uint16_t c) const override;
        [[nodiscard]] std::uint16_t getRowUnknownCount(std::uint16_t r) const override;
        [[nodiscard]] std::array<std::uint64_t, 8> getRow(std::uint16_t r) const override;

    private:
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
         * @name rowStats_
         * @brief Per-row line statistics.
         */
        std::vector<LineStat> rowStats_;

        /**
         * @name colStats_
         * @brief Per-column line statistics.
         */
        std::vector<LineStat> colStats_;

        /**
         * @name diagStats_
         * @brief Per-diagonal line statistics.
         */
        std::vector<LineStat> diagStats_;

        /**
         * @name antiDiagStats_
         * @brief Per-anti-diagonal line statistics.
         */
        std::vector<LineStat> antiDiagStats_;

        /**
         * @name rowBits_
         * @brief Row bit storage for hash verification (8 x uint64 per row, MSB-first).
         */
        std::vector<std::array<std::uint64_t, 8>> rowBits_;
    };
} // namespace crsce::decompress::solvers
