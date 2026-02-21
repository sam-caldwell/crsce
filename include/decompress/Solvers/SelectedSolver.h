/**
 * @file SelectedSolver.h
 * @author Sam Caldwell
 * @brief Factory for constructing the primary decompressor solver (PipelineSolver).
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <memory>
#include <span>

#include "decompress/Solvers/GenericSolver.h"
#include "decompress/CrossSum/CrossSums.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Solvers/PipelineSolver/PipelineSolver.h"

namespace crsce::decompress::solvers::selected {
    /**
     * @name make_primary_solver
     * @brief Construct the primary solver (PipelineSolver) with explicit cross-sums and LH payload.
     * @param csm Constraint/state matrix for the block.
     * @param st ConstraintState for the block.
     * @param sums Cross-sum bundle for the block.
     * @param lh Little-header payload bytes for the block.
     * @return std::unique_ptr<GenericSolver> owning a PipelineSolver instance.
     */
    inline std::unique_ptr<GenericSolver>
    make_primary_solver(Csm &csm,
                        ConstraintState &st,
                        const CrossSums &sums,
                        std::span<const std::uint8_t> lh) {
        using ::crsce::decompress::solvers::pipeline::PipelineSolver;
        return std::make_unique<PipelineSolver>(csm, st, sums, lh);
    }
}
