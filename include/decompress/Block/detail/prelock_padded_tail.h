/**
 * @file prelock_padded_tail.h
 * @brief Lock padded tail cells and update unknown counters.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"

namespace crsce::decompress::detail {
    /**
     * @name prelock_padded_tail
     * @brief Set and lock all cells beyond `valid_bits` as zero and decrement unknown counts.
     * @param csm Cross‑Sum Matrix to mutate.
     * @param st Constraint state to update (unknowns per family).
     * @param valid_bits Number of meaningful cells (row-major order).
     */
    void prelock_padded_tail(Csm &csm, ConstraintState &st, std::uint64_t valid_bits);
}
