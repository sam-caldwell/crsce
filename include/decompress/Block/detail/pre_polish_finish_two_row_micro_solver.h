/**
 * @file pre_polish_finish_two_row_micro_solver.h
 * @brief Declare two-row boundary micro-solver.
 */
#pragma once

#include <span>
#include <cstdint>
#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    bool finish_two_row_micro_solver(Csm &csm_out,
                                     ConstraintState &st,
                                     std::span<const std::uint8_t> lh,
                                     Csm &baseline_csm,
                                     ConstraintState &baseline_st,
                                     BlockSolveSnapshot &snap,
                                     int rs);
}

