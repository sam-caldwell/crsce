/**
 * @file DeterministicElimination.h
 * @brief Deterministic elimination solver implementing sound forced moves.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "decompress/Csm/Csm.h"
#include "decompress/Solver/Solver.h"
#include "decompress/DeterministicElimination/ConstraintState.h"

namespace crsce::decompress {
    // ConstraintState moved to a dedicated header to satisfy one-definition-per-header.

    /**
     * @class DeterministicElimination
     * @name DeterministicElimination
     * @brief Enforces forced moves: if R=0 -> all remaining vars become 0; if R=U -> all remaining vars become 1.
     *        Maintains feasibility (0 ≤ R ≤ U) for all four line families; throws on contradictions.
     */
    class DeterministicElimination : public Solver {
    public:
        /**
         * @name DeterministicElimination::DeterministicElimination
         * @brief Construct a deterministic elimination solver over a CSM.
         * @param csm Target cross-sum matrix to solve.
         * @param state Residual constraints (R and U across all lines).
         */
        DeterministicElimination(Csm &csm, ConstraintState &state);

        /**
         * @name DeterministicElimination::solve_step
         * @brief Perform one pass of forced-move propagation.
         * @return std::size_t Number of newly solved cells this step.
         */
        std::size_t solve_step() override;

        /**
         * @name DeterministicElimination::solved
         * @brief Whether the matrix is fully solved (no undecided cells remain).
         * @return bool True if solved; false otherwise.
         */
        [[nodiscard]] bool solved() const override;

        /**
         * @name DeterministicElimination::reset
         * @brief Reset solver state and clear all locks/data.
         * @return void
         */
        void reset() override;

        // Hash-based deterministic elimination removed: chaining prevents safe use during assignment.

    private:
        /**
         * @name csm_
         * @brief Reference to the target Cross-Sum Matrix being solved.
         */
        Csm &csm_;

        /**
         * @name st_
         * @brief Residual constraint state (R and U for row/col/diag/xdiag).
         */
        ConstraintState &st_;

        static constexpr std::size_t S = Csm::kS;

        /**
         * @name diag_index
         * @brief Compute diagonal index for (r,c).
         * @param r Row index.
         * @param c Column index.
         * @return d = (r + c) mod S.
         */
        static constexpr std::size_t diag_index(std::size_t r, std::size_t c) noexcept {
            return (r + c) % S;
        }

        /**
         * @name xdiag_index
         * @brief Compute anti-diagonal index for (r,c).
         * @param r Row index.
         * @param c Column index.
         * @return x = (r >= c) ? (r - c) : (r + S - c).
         */
        static constexpr std::size_t xdiag_index(std::size_t r, std::size_t c) noexcept {
            return (r >= c) ? (r - c) : (r + S - c);
        }

        /**
         * @name DeterministicElimination::validate_bounds
         * @brief Validate feasibility 0 ≤ R ≤ U across all constraint lines.
         * @param st Constraint state to validate.
         * @return void
         */
        static void validate_bounds(const ConstraintState &st);

        /**
         * @name DeterministicElimination::force_row
         * @brief Force all undecided cells in row r to value.
         * @param r Row index.
         * @param value Forced value for undecided cells.
         * @param progress Incremented by number of newly solved cells.
         * @return void
         */
        void force_row(std::size_t r, bool value, std::size_t &progress);

        /**
         * @name DeterministicElimination::force_col
         * @brief Force all undecided cells in column c to value.
         * @param c Column index.
         * @param value Forced value for undecided cells.
         * @param progress Incremented by number of newly solved cells.
         * @return void
         */
        void force_col(std::size_t c, bool value, std::size_t &progress);

        /**
         * @name DeterministicElimination::force_diag
         * @brief Force all undecided cells on diagonal d to value.
         * @param d Diagonal index.
         * @param value Forced value for undecided cells.
         * @param progress Incremented by number of newly solved cells.
         * @return void
         */
        void force_diag(std::size_t d, bool value, std::size_t &progress);

        /**
         * @name DeterministicElimination::force_xdiag
         * @brief Force all undecided cells on anti-diagonal x to value.
         * @param x Anti-diagonal index.
         * @param value Forced value for undecided cells.
         * @param progress Incremented by number of newly solved cells.
         * @return void
         */
        void force_xdiag(std::size_t x, bool value, std::size_t &progress);

        /**
         * @name DeterministicElimination::apply_cell
         * @brief Assign value to (r,c), update constraints, and lock cell.
         * @param r Row index.
         * @param c Column index.
         * @param value Assigned value for the cell.
         * @return void
         */
        void apply_cell(std::size_t r, std::size_t c, bool value);
    };
} // namespace crsce::decompress
