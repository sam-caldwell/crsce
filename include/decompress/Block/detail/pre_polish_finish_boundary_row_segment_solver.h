/**
 * @file pre_polish_finish_boundary_row_segment_solver.h
 * @brief Try to finish boundary row using a sliding-window segment solver (DP/BnB) under constraints.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

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
