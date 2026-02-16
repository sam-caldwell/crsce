/**
 * @file DeterministicElimination_ctor.cpp
 * @brief Implementation of DeterministicElimination constructor (phase).
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Phases/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress {
    /**
     * @name DeterministicElimination::DeterministicElimination
     * @brief Construct a deterministic elimination solver over a CSM.
     * @param kMaxIters maximum number of iterations
     * @param csm Target cross-sum matrix to solve.
     * @param state Residual constraints (R and U across all lines).
     * @param snap
     * @param lh
     * @return void
     */
    DeterministicElimination::DeterministicElimination(
        const std::uint64_t kMaxIters,
        Csm &csm, ConstraintState &state,
        BlockSolveSnapshot &snap,
        const std::span<const uint8_t> &lh) : kMaxIters(kMaxIters),
                                              csm_(csm),
                                              st_(state),
                                              snap_(snap),
                                              lh_(lh) {
        validate_bounds(st_);
    }
} // namespace crsce::decompress
