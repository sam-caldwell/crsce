/**
 * @file pre_polish_finish_near_complete_top_columns.h
 * @brief Declare finish_near_complete_top_columns helper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <span>
#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    /**
     * @brief Finish near-complete top-N columns under constraints.
     */
    bool finish_near_complete_top_columns(Csm &csm_out,
                                          ConstraintState &st,
                                          std::span<const std::uint8_t> lh,
                                          Csm &baseline_csm,
                                          ConstraintState &baseline_st,
                                          BlockSolveSnapshot &snap,
                                          int rs,
                                          std::size_t top_n_cols,
                                          std::size_t top_k_cells);
}
