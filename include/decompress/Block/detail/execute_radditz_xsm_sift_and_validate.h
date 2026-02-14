/**
 * @file execute_radditz_xsm_sift_and_validate.h
 * @brief XSM-focused 2x2 swap sift to reduce anti-diagonal deficits while preserving row/col.
 */
#pragma once

#include <span>
#include <cstdint>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    bool execute_radditz_xsm_sift_and_validate(Csm &csm,
                                               ConstraintState &st,
                                               BlockSolveSnapshot &snap,
                                               std::span<const std::uint16_t> xsm,
                                               std::span<const std::uint8_t> lh);
}
