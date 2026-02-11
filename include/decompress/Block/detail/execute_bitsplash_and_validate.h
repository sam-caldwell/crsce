/**
 * @file execute_bitsplash_and_validate.h
 * @brief Run BitSplash and validate row sums vs LSM.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <span>
#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    bool execute_bitsplash_and_validate(Csm &csm,
                                        ConstraintState &st,
                                        BlockSolveSnapshot &snap,
                                        std::span<const std::uint16_t> lsm);
}

