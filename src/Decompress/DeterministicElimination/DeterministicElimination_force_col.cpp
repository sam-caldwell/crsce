/**
 * @file DeterministicElimination_force_col.cpp
 * @brief Implementation of DeterministicElimination::force_col.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name DeterministicElimination::force_col
     * @brief Force all undecided cells in column c to value.
     * @param c Column index.
     * @param value Forced value for undecided cells.
     * @param progress Incremented by number of newly solved cells.
     * @return void
     */
    void DeterministicElimination::force_col(const std::size_t c, const bool value, std::size_t &progress) {
        for (std::size_t r = 0; r < S; ++r) {
            const auto before_locked = csm_.is_locked(r, c);
            apply_cell(r, c, value);
            if (!before_locked) {
                ++progress;
            }
        }
    }
} // namespace crsce::decompress
