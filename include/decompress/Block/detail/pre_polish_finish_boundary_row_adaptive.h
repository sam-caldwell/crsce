/**
 * @file pre_polish_finish_boundary_row_adaptive.h
 * @brief Declare finish_boundary_row_adaptive helper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <span>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    /**
     * @brief Adaptively finish the current boundary row if possible.
     */
    bool finish_boundary_row_adaptive(Csm &csm_out,
                                      ConstraintState &st,
                                      std::span<const std::uint8_t> lh,
                                      Csm &baseline_csm,
                                      ConstraintState &baseline_st,
                                      BlockSolveSnapshot &snap,
                                      int rs,
                                      int stall_ticks);
}
