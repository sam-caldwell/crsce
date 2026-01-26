/**
 * @file GobpSolver.h
 * @brief CPU-based, single-host Generalized Ordered Belief Propagation (GOBP) solver.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/ConstraintState.h"
#include "decompress/Solver/Solver.h"

namespace crsce::decompress {
    /**
     * @class GobpSolver
     * @name GobpSolver
     * @brief Implements a simple CPU GOBP pass using naive odds-product factor combination
     *        across row/column/diag/xdiag constraints. Writes smoothed beliefs into CSM data layer
     *        and applies deterministic assignments when belief crosses confidence thresholds.
     */
    class GobpSolver : public Solver {
    public:
        /**
         * @name GobpSolver::GobpSolver
         * @brief Construct a GOBP solver over a CSM.
         * @param csm Target CSM to update (bits/locks and data layer for beliefs)
         * @param state Line residual constraints (R and U for row/col/diag/xdiag)
         * @param damping Exponential smoothing on beliefs in [0,1). 0=no smoothing, 0.5=blend.
         * @param assign_confidence Threshold in (0.5,1] to assign 1 if >=, 0 if <= 1-threshold.
         */
        GobpSolver(Csm &csm, ConstraintState &state,
                   double damping = 0.5,
                   double assign_confidence = 0.995);

        /**
         * @name GobpSolver::solve_step
         * @brief Perform one iteration of GOBP smoothing and assignments.
         * @return std::size_t Number of newly solved cells.
         */
        std::size_t solve_step() override;

        /**
         * @name GobpSolver::solved
         * @brief Whether the target matrix is fully solved.
         * @return bool True if solved; false otherwise.
         */
        [[nodiscard]] bool solved() const override;

        /**
         * @name GobpSolver::reset
         * @brief Reset internal state and parameters to defaults.
         * @return void
         */
        void reset() override;

        // Accessors for parameters
        /**
         * @name damping
         * @brief Current exponential smoothing in [0,1).
         * @return Damping factor.
         */
        [[nodiscard]] double damping() const noexcept { return damping_; }

        /**
         * @name assign_confidence
         * @brief Current confidence threshold in (0.5,1].
         * @return Assignment confidence.
         */
        [[nodiscard]] double assign_confidence() const noexcept { return assign_confidence_; }

        /**
         * @name GobpSolver::set_damping
         * @brief Update exponential smoothing factor.
         * @param d New damping value in [0,1).
         * @return void
         */
        void set_damping(double d) noexcept { damping_ = clamp01(d); }

        /**
         * @name GobpSolver::set_assign_confidence
         * @brief Update confidence threshold used for assignments.
         * @param c New threshold in (0.5,1].
         * @return void
         */
        void set_assign_confidence(double c) noexcept { assign_confidence_ = clamp_conf(c); }

    private:
        /**
         * @name csm_
         * @brief Reference to the target Cross-Sum Matrix being solved.
         */
        Csm &csm_;

        /**
         * @name st_
         * @brief Residual constraint state used to derive line beliefs.
         */
        ConstraintState &st_;

        /**
         * @name damping_
         * @brief Exponential smoothing parameter in [0,1).
         */
        double damping_{0.5};

        /**
         * @name assign_confidence_
         * @brief Threshold in (0.5,1] for making deterministic assignments.
         */
        double assign_confidence_{0.995};

        static constexpr std::size_t S = Csm::kS;

        // Utility: compute indexes for diagonal families
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

        // Probability utilities
        /**
         * @name GobpSolver::clamp01
         * @brief Clamp probability value to [0,1].
         * @param v Input value.
         * @return double Clamped value in [0,1].
         */
        static double clamp01(double v) noexcept;

        /**
         * @name GobpSolver::clamp_conf
         * @brief Clamp assignment confidence to (0.5,1].
         * @param v Input value.
         * @return double Clamped value in (0.5,1].
         */
        static double clamp_conf(double v) noexcept;

        /**
         * @name GobpSolver::odds
         * @brief Convert probability to odds ratio p/(1-p).
         * @param p Probability in (0,1).
         * @return double Odds value.
         */
        static double odds(double p) noexcept; // p -> p/(1-p)

        /**
         * @name GobpSolver::prob_from_odds
         * @brief Convert odds ratio to probability o/(1+o).
         * @param o Odds value > 0.
         * @return double Probability in (0,1).
         */
        static double prob_from_odds(double o) noexcept; // o -> o/(1+o)

        // Compute per-cell belief based on current residuals (naive independent combination)
        /**
         * @name GobpSolver::belief_for
         * @brief Compute per-cell belief based on current residuals.
         * @param r Row index.
         * @param c Column index.
         * @return double Belief value in [0,1].
         */
        [[nodiscard]] double belief_for(std::size_t r, std::size_t c) const;

        // Apply a concrete assignment to a cell, updating residuals and locking the cell.
        /**
         * @name GobpSolver::apply_cell
         * @brief Apply concrete assignment to (r,c), update residuals, lock cell.
         * @param r Row index.
         * @param c Column index.
         * @param value Assigned value.
         * @return void
         */
        void apply_cell(std::size_t r, std::size_t c, bool value);
    };
} // namespace crsce::decompress
