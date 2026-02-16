/**
 * @file pre_polish_finish_near_complete_any_row.h
 * @brief Declare finish_near_complete_any_row helper.
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
     * @brief Finish any row that is nearly complete under current constraints.
     */
    bool finish_near_complete_any_row(Csm &csm_out,
                                      ConstraintState &st,
                                      std::span<const std::uint8_t> lh,
                                      Csm &baseline_csm,
                                      ConstraintState &baseline_st,
                                      BlockSolveSnapshot &snap,
                                      int rs);
}
