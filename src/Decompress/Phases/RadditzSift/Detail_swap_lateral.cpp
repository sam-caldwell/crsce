/**
 * @file Detail_swap_lateral.cpp
 * @brief swap_lateral for Radditz helpers.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <cstddef>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/RadditzSift/swap_lateral.h"

namespace crsce::decompress::phases::detail {
    /**
     * @name swap_lateral
     * @brief Attempt 1@cf→0 and 0@ct→1 swap on the same row; reject if invalid/locked.
     *        Does not set resolved-locks; uses non-locking writes (no MU guard) for speed.
     * @param csm Matrix to modify.
     * @param r Row index.
     * @param cf Donor column (must be 1 and unlocked).
     * @param ct Target column (must be 0 and unlocked).
     * @return bool True if swap performed; otherwise false.
     */
    bool swap_lateral(Csm &csm, const std::size_t r, const std::size_t cf, const std::size_t ct) {
        if (cf == ct) { return false; }
        if (csm.is_locked(r, cf) || csm.is_locked(r, ct)) { return false; }
        const bool b_from = csm.get(r, cf);
        const bool b_to   = csm.get(r, ct);
        if (!b_from || b_to) { return false; }
        // Use MU-guarded writes for atomicity at the cell level.
        csm.clear(r, cf);   // default lock = Locked
        csm.set(r, ct);     // default lock = Locked
        return true;
    }
}
