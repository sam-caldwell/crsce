/**
 * @file pre_polish_finish_near_complete_columns_scored.h
 * @brief Declare finish_near_complete_columns_scored helper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <span>
#include <vector>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    /**
     * @brief Finish near-complete columns prioritized by score ordering.
     */
    bool finish_near_complete_columns_scored(Csm &csm_out,
                                             ConstraintState &st,
                                             std::span<const std::uint8_t> lh,
                                             Csm &baseline_csm,
                                             ConstraintState &baseline_st,
                                             BlockSolveSnapshot &snap,
                                             int rs,
                                             const std::vector<std::size_t> &cols,
                                             std::size_t top_k_cells);
}
