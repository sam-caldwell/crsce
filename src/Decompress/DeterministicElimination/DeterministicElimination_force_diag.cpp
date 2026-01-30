/**
 * @file DeterministicElimination_force_diag.cpp
 * @brief Implementation of DeterministicElimination::force_diag.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name DeterministicElimination::force_diag
     * @brief Force all undecided cells on diagonal d to value.
     * @param d Diagonal index.
     * @param value Forced value for undecided cells.
     * @param progress Incremented by number of newly solved cells.
     * @return void
     */
    void DeterministicElimination::force_diag(const std::size_t d, const bool value, std::size_t &progress) {
        for (std::size_t r = 0; r < S; ++r) {
            const std::size_t c = (d + S - (r % S)) % S;
            const auto before_locked = csm_.is_locked(r, c);
            apply_cell(r, c, value);
            if (!before_locked) {
                ++progress;
            }
        }
    }
} // namespace crsce::decompress
