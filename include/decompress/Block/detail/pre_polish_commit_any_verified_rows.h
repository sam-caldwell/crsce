/**
 * @file pre_polish_commit_any_verified_rows.h
 * @brief Declare commit_any_verified_rows helper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <span>
#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    bool commit_any_verified_rows(Csm &csm_out,
                                  ConstraintState &st,
                                  std::span<const std::uint8_t> lh,
                                  Csm &baseline_csm,
                                  ConstraintState &baseline_st,
                                  BlockSolveSnapshot &snap,
                                  int rs);
}
