/**
 * @file pre_polish_commit_valid_prefix.h
 * @brief Declare commit_valid_prefix helper.
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
     * @brief Commit the longest currently valid prefix of bits.
     */
    bool commit_valid_prefix(Csm &csm_out,
                             ConstraintState &st,
                             std::span<const std::uint8_t> lh,
                             Csm &baseline_csm,
                             ConstraintState &baseline_st,
                             BlockSolveSnapshot &snap,
                             int rs);
}
