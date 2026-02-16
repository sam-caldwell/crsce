/**
 * @file pre_polish_finish_boundary_row_segment_solver.h
 * @brief Public declaration for boundary row segment solver (forwards to detail).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once
#include "decompress/Block/detail/pre_polish_finish_boundary_row_segment_solver.h"

namespace crsce::decompress { class Csm; }
namespace crsce::decompress { struct ConstraintState; }
namespace crsce::decompress { struct BlockSolveSnapshot; }

namespace crsce::decompress::detail {
    /**
     * @brief Attempt to finish the boundary row using a segment solver.
     */
    bool finish_boundary_row_segment_solver(Csm &csm_out,
                                            ConstraintState &st,
                                            std::span<const std::uint8_t> lh,
                                            Csm &baseline_csm,
                                            ConstraintState &baseline_st,
                                            BlockSolveSnapshot &snap,
                                            int rs);
}
