/**
 * @file pre_polish_finish_near_complete_any_row.h
 * @brief Public declaration for finishing any near-complete row (forwards to detail).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once
#include "decompress/Block/detail/pre_polish_finish_near_complete_any_row.h"

namespace crsce::decompress { class Csm; }
namespace crsce::decompress { struct ConstraintState; }
namespace crsce::decompress { struct BlockSolveSnapshot; }

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
