/**
 * @file ConstraintSolver.h
 * @brief Residual-driven constraint solver orchestrator (DE → BitSplash → Radditz → Hybrid Accel).
 *        This solver mirrors the existing pipeline but routes the final sift through an
 *        acceleration wrapper that may leverage Apple Metal (when available) to rank
 *        2x2 rectangle candidates. CPU fallback is automatic.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <span>
#include <cstdint>

#include "decompress/Csm/Csm.h"
#include "decompress/CrossSum/CrossSums.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Solvers/GenericSolver.h"

namespace crsce::decompress::solvers::constraint {
    /**
     * @class ConstraintSolver
     * @brief Executes DE → BitSplash → Radditz → Hybrid(Accel) within GenericSolver interface.
     */
    class ConstraintSolver : public ::crsce::decompress::solvers::GenericSolver {
    public:
        /**
         * @brief Construct a ConstraintSolver bound to a block's CSM/state and targets.
         * @param csm Cross‑Sum Matrix to update.
         * @param st ConstraintState residuals/unknowns for the block.
         * @param sums Cross‑sum targets bundle (LSM,VSM,DSM,XSM).
         * @param lh Little‑header payload for this block (row hash matrix backing).
         */
        ConstraintSolver(Csm &csm,
                         ConstraintState &st,
                         const CrossSums &sums,
                         std::span<const std::uint8_t> lh) noexcept;

        /**
         * @brief One step not used; returns 0 to indicate composite passes only.
         * @return std::size_t Progress units (always 0 for composite solver).
         */

        /**
         * @brief Execute the full residual‑driven pipeline once; ignores max_iters.
         * @param max_iters Ignored; present for interface compatibility.
         */
        void solve(std::size_t max_iters = 0) override;

    private:
        const CrossSums &sums_;
        std::span<const std::uint8_t> lh_;
    };
}
