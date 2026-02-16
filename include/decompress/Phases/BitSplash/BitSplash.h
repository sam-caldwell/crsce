/**
 * @file BitSplash.h
 * @brief BitSplash phase: rows-only placement to match LSM per row without locking.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::phases {
    /**
     * @name bit_splash
     * @brief Rows-only placement: for each row r, set/unset bits on unlocked cells so
     *        sum(CSM[r,*]) == LSM[r]. No locking. Throws if locked ones exceed LSM[r].
     * @param csm CSM to update.
     * @param st Constraint state (unused by BitSplash; kept for signature consistency).
     * @param snap Snapshot (uses snap.lsm for targets; updates timing and counters).
     * @param max_rows Optional cap on rows processed (0 = all).
     * @return std::size_t Number of bit flips performed.
     */
    std::size_t bit_splash(Csm &csm,
                           ConstraintState &st,
                           BlockSolveSnapshot &snap,
                           std::size_t max_rows = 0);
}
