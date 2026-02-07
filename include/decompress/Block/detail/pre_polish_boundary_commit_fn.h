/**
 * @file pre_polish_boundary_commit_fn.h
 * @brief Declare pre_polish_boundary_commit helper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <span>
#include <string>
#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    bool pre_polish_boundary_commit(Csm &csm_out,
                                    ConstraintState &st,
                                    std::span<const std::uint8_t> lh,
                                    const std::string &seed,
                                    Csm &baseline_csm,
                                    ConstraintState &baseline_st,
                                    BlockSolveSnapshot &snap,
                                    int rs);
}

