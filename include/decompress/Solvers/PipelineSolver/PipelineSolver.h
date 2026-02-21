/**
 * @file PipelineSolver.h
 * @brief Primary solver orchestrating DE → BitSplash → Radditz → Hybrid Sift.
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

namespace crsce::decompress::solvers::pipeline {
    /**
     * @class PipelineSolver
     * @brief Executes the decompression pipeline (DE → BitSplash → Radditz → Hybrid Sift)
     *        within the GenericSolver interface.
     */
    class PipelineSolver : public ::crsce::decompress::solvers::GenericSolver {
    public:
        PipelineSolver(Csm &csm,
                       ConstraintState &st,
                       const CrossSums &sums,
                       std::span<const std::uint8_t> lh) noexcept
            : GenericSolver(csm, st), sums_(sums), lh_(lh) {}

        // Not used: pipeline solves in one composite pass; returns 0 progress
        std::size_t solve_step() override { return 0; }

        // Execute the full pipeline once; ignore max_iters
        void solve(std::size_t /*max_iters*/ = 0) override;

    private:
        const CrossSums &sums_;
        std::span<const std::uint8_t> lh_;
    };
}
