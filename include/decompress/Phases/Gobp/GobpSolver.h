/**
 * @file GobpSolver.h
 * @brief CPU-based, single-host Game Of Beliefs Protocol (GOBP) solver.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/ConstraintState.h"
#include "decompress/Solver/Solver.h"
#include "decompress/Utils/detail/index_calc_d.h"
#include "decompress/Utils/detail/index_calc_x.h"

namespace crsce::decompress {
    /**
     * @class GobpSolver
     * @brief CPU GOBP pass using naive odds-product factor combination across constraints.
     */
    class GobpSolver : public Solver {
    public:
        /** @brief Construct a GOBP solver over a CSM and constraint state. */
        GobpSolver(Csm &csm, ConstraintState &state,
                   double damping = 0.5,
                   double assign_confidence = 0.995);
        /** @brief Perform one iteration of smoothing and assignments. */
        std::size_t solve_step() override;
        /** @brief Whether the matrix is solved (no unknowns across families). */
        [[nodiscard]] bool solved() const override;
        /** @brief Reset internal parameters to defaults. */
        void reset() override;

        /** @brief Current damping parameter. */
        [[nodiscard]] double damping() const noexcept { return damping_; }
        /** @brief Current assignment confidence threshold. */
        [[nodiscard]] double assign_confidence() const noexcept { return assign_confidence_; }
        /** @brief Update damping parameter. */
        void set_damping(double d) noexcept { damping_ = clamp01(d); }
        /** @brief Update assignment confidence threshold. */
        void set_assign_confidence(double c) noexcept { assign_confidence_ = clamp_conf(c); }
        /** @brief Toggle alternate scan order for plateau escape. */
        void set_scan_flipped(bool v) noexcept { scan_flipped_ = v; }

    private:
        /** @brief Target matrix reference. */
        Csm &csm_;
        /** @brief Constraint state reference. */
        ConstraintState &st_;
        /** @brief Exponential smoothing factor in [0,1). */
        double damping_{0.5};
        /** @brief Threshold in (0.5,1] for deterministic assignments. */
        double assign_confidence_{0.995};
        /** @brief Matrix size shorthand. */
        static constexpr std::size_t S = Csm::kS;
        /** @brief When true, use alternate scan order. */
        bool scan_flipped_{false};

        /** @brief Compute diagonal index for (r,c) via branchless helper. */
        static inline std::size_t diag_index(std::size_t r, std::size_t c) noexcept {
            return detail::calc_d(r, c);
        }
        /** @brief Compute anti-diagonal index for (r,c) via branchless helper. */
        static inline std::size_t xdiag_index(std::size_t r, std::size_t c) noexcept {
            return detail::calc_x(r, c);
        }

        /** @brief Clamp probability to [0,1]. */
        static double clamp01(double v) noexcept;
        /** @brief Clamp confidence to (0.5,1]. */
        static double clamp_conf(double v) noexcept;
        /** @brief Convert probability to odds p/(1-p). */
        static double odds(double p) noexcept;
        /** @brief Convert odds to probability o/(1+o). */
        static double prob_from_odds(double o) noexcept;
        /** @brief Compute belief for (r,c) under current residuals. */
        [[nodiscard]] double belief_for(std::size_t r, std::size_t c) const;
        /** @brief Helper for belief computation given (R,U). */
        static auto calculate_belief(std::size_t R, std::size_t U) -> double;
        /** @brief Apply a concrete assignment and update residuals; lock cell. */
        void apply_cell(std::size_t r, std::size_t c, bool value);
    };
} // namespace crsce::decompress
