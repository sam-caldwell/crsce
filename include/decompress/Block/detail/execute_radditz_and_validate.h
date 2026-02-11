/**
 * @file execute_radditz_and_validate.h
 * @brief Run Radditz Sift and validate column sums vs VSM + row invariant vs LSM.
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
    bool execute_radditz_and_validate(Csm &csm,
                                      ConstraintState &st,
                                      BlockSolveSnapshot &snap,
                                      std::span<const std::uint16_t> lsm,
                                      std::span<const std::uint16_t> vsm);
}

