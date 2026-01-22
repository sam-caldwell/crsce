/**
 * @file GobpSolver.h
 * @brief CPU-based, single-host Generalized Ordered Belief Propagation (GOBP) solver.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h" // for ConstraintState
#include "decompress/Solver/Solver.h"

namespace crsce::decompress {

    /**
     * @class GobpSolver
     * @brief Implements a simple CPU GOBP pass using naive odds-product factor combination
     *        across row/column/diag/xdiag constraints. Writes smoothed beliefs into CSM data layer
     *        and applies deterministic assignments when belief crosses confidence thresholds.
     */
    class GobpSolver : public Solver {
    public:
        /**
         * @param csm Target CSM to update (bits/locks and data layer for beliefs)
         * @param state Line residual constraints (R and U for row/col/diag/xdiag)
         * @param damping Exponential smoothing on beliefs in [0,1). 0=no smoothing, 0.5=blend.
         * @param assign_confidence Threshold in (0.5,1] to assign 1 if >=, 0 if <= 1-threshold.
         */
        GobpSolver(Csm &csm, ConstraintState &state,
                   double damping = 0.5,
                   double assign_confidence = 0.995);

        std::size_t solve_step() override;

        [[nodiscard]] bool solved() const override;

        void reset() override;

        // Accessors for parameters
        [[nodiscard]] double damping() const noexcept { return damping_; }
        [[nodiscard]] double assign_confidence() const noexcept { return assign_confidence_; }

        void set_damping(double d) noexcept { damping_ = clamp01(d); }
        void set_assign_confidence(double c) noexcept { assign_confidence_ = clamp_conf(c); }

    private:
        Csm &csm_;
        ConstraintState &st_;
        double damping_{0.5};
        double assign_confidence_{0.995};

        static constexpr std::size_t S = Csm::kS;

        // Utility: compute indexes for diagonal families
        static constexpr std::size_t diag_index(std::size_t r, std::size_t c) noexcept {
            return (r + c) % S;
        }
        static constexpr std::size_t xdiag_index(std::size_t r, std::size_t c) noexcept {
            return (r >= c) ? (r - c) : (r + S - c);
        }

        // Probability utilities
        static double clamp01(double v) noexcept;
        static double clamp_conf(double v) noexcept;
        static double odds(double p) noexcept;            // p -> p/(1-p)
        static double prob_from_odds(double o) noexcept;  // o -> o/(1+o)

        // Compute per-cell belief based on current residuals (naive independent combination)
        [[nodiscard]] double belief_for(std::size_t r, std::size_t c) const;

        // Apply a concrete assignment to a cell, updating residuals and locking the cell.
        void apply_cell(std::size_t r, std::size_t c, bool value);
    };
} // namespace crsce::decompress
