/**
 * @file DeterministicElimination_force_xdiag.cpp
 * @brief Implementation of DeterministicElimination::force_xdiag.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name DeterministicElimination::force_xdiag
     * @brief Force all undecided cells on anti-diagonal x to value.
     * @param x Anti-diagonal index.
     * @param value Forced value for undecided cells.
     * @param progress Incremented by number of newly solved cells.
     * @return void
     */
    void DeterministicElimination::force_xdiag(const std::size_t x, const bool value, std::size_t &progress) {
        for (std::size_t r = 0; r < S; ++r) {
            const std::size_t c = (r + S - (x % S)) % S;
            const auto before_locked = csm_.is_locked(r, c);
            apply_cell(r, c, value);
            if (!before_locked) {
                ++progress;
            }
        }
    }
} // namespace crsce::decompress
