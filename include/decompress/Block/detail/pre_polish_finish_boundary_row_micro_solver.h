/**
 * @file pre_polish_finish_boundary_row_micro_solver.h
 * @brief Declare finish_boundary_row_micro_solver helper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <span>
#include <cstdint>
#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    // Heuristic micro-solver: try to complete the boundary row using residual constraints,
    // adopt only if the extended prefix verifies against LH.
    bool finish_boundary_row_micro_solver(Csm &csm_out,
                                          ConstraintState &st,
                                          std::span<const std::uint8_t> lh,
                                          Csm &baseline_csm,
                                          ConstraintState &baseline_st,
                                          BlockSolveSnapshot &snap,
                                          int rs);
}

