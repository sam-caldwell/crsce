/**
 * @file SelectedSolver.cpp
 * @brief Factory implementation for the primary decompressor solver.
 *        Always returns ConstraintSolver (pipeline removed).
 * @author Sam Caldwell
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */

#include "decompress/Solvers/SelectedSolver.h"

#include <memory>
#include <span>

#include "decompress/Solvers/GenericSolver.h"
#include "decompress/CrossSum/CrossSums.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"

#ifdef CRSCE_SOLVER_CONSTRAINT
#  ifdef CRSCE_SOLVER_PIPELINE
#error "Both CRSCE_SOLVER_CONSTRAINT and CRSCE_SOLVER_PIPELINE are defined; choose exactly one."
#  endif
#endif

#include <cstdint>

#include "decompress/Solvers/ConstraintSolver/ConstraintSolver.h"

namespace crsce::decompress::solvers::selected {

std::unique_ptr<GenericSolver>
make_primary_solver(Csm &csm,
                    ConstraintState &st,
                    const CrossSums &sums,
                    std::span<const std::uint8_t> lh) {
    using ::crsce::decompress::solvers::constraint::ConstraintSolver;
    return std::make_unique<ConstraintSolver>(csm, st, sums, lh);
}

} // namespace crsce::decompress::solvers::selected
