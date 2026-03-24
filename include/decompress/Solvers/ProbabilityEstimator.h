/**
 * @file ProbabilityEstimator.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Computes per-cell probability estimates from cross-line residuals for guided branching.
 */
#pragma once

#include <cstdint>
#include <vector>

#include "decompress/Solvers/ConstraintStore.h"

namespace crsce::decompress::solvers {

    /**
     * @struct CellScore
     * @name CellScore
     * @brief Per-cell probability estimate derived from 7 non-row line residuals.
     */
    struct CellScore {
        /**
         * @name row
         * @brief Row index of this cell.
         */
        std::uint16_t row;

        /**
         * @name col
         * @brief Column index of this cell.
         */
        std::uint16_t col;

        /**
         * @name score1
         * @brief Product of 7 rho values (col, diag, anti-diag, 4 slopes), proportional to P(cell=1).
         */
        std::uint64_t score1;

        /**
         * @name score0
         * @brief Product of 7 (u-rho) values, proportional to P(cell=0).
         */
        std::uint64_t score0;

        /**
         * @name preferred
         * @brief Preferred branch value: 1 if score1 > score0, else 0.
         */
        std::uint8_t preferred;

        /**
         * @name confidence
         * @brief Absolute difference |score1 - score0| for sorting (higher = more confident).
         */
        std::uint64_t confidence;
    };

    /**
     * @class ProbabilityEstimator
     * @name ProbabilityEstimator
     * @brief Computes per-cell P(1) estimates from cross-line residuals for a given row.
     *
     * Uses only the 7 non-row lines (column, diagonal, anti-diagonal, and 4 toroidal
     * slopes) to estimate how likely each unknown cell is to be 1 vs 0. The scores
     * are products of 7 residuals, bounded by 511^7 < 2^63, so uint64_t arithmetic suffices.
     */
    class ProbabilityEstimator {
    public:
        /**
         * @name kS
         * @brief Matrix dimension.
         */
        static constexpr std::uint16_t kS = 127;

        /**
         * @name ProbabilityEstimator
         * @brief Construct an estimator bound to a constraint store.
         * @param store Const reference to the constraint store (must outlive this estimator).
         * @throws None
         */
        explicit ProbabilityEstimator(const ConstraintStore &store);

        /**
         * @name computeCellScores
         * @brief Compute probability-guided scores for all unassigned cells in row r.
         *
         * For each unassigned cell (r, c), reads the column, diagonal, and anti-diagonal
         * line statistics to compute score1 (favoring 1) and score0 (favoring 0).
         * Returns cells sorted by confidence descending (most constrained first).
         *
         * @param r Row index in [0, kS).
         * @return Vector of CellScore sorted by confidence descending.
         * @throws None
         */
        [[nodiscard]] std::vector<CellScore> computeCellScores(std::uint16_t r) const;

        /**
         * @name computeGlobalCellScores
         * @brief Compute probability-guided scores for all unassigned cells across all rows.
         *
         * Iterates every row, scoring each unassigned cell using column, diagonal, and
         * anti-diagonal residuals. Returns all cells in a single vector sorted by
         * confidence descending (most constrained first).
         *
         * @return Vector of CellScore for all unassigned cells, sorted by confidence descending.
         * @throws None
         */
        [[nodiscard]] std::vector<CellScore> computeGlobalCellScores() const;

    private:
        /**
         * @name store_
         * @brief Reference to the constraint store for reading line statistics.
         */
        const ConstraintStore &store_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    };

} // namespace crsce::decompress::solvers
