/**
 * @file DeterministicElimination_force_row.cpp
 * @brief Implementation of DeterministicElimination::force_row.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name DeterministicElimination::force_row
     * @brief Force all undecided cells in row r to value.
     * @param r Row index.
     * @param value Forced value for undecided cells.
     * @param progress Incremented by number of newly solved cells.
     * @return void
     */
    void DeterministicElimination::force_row(const std::size_t r, const bool value, std::size_t &progress) {
        const auto to_assign = static_cast<std::size_t>(st_.U_row.at(r));
        std::size_t assigned = 0;
        for (std::size_t c = 0; c < S; ++c) {
            if (assigned >= to_assign) { break; }
            if (csm_.is_locked(r, c)) { continue; }
            apply_cell(r, c, value);
            ++progress;
            ++assigned;
        }
    }
} // namespace crsce::decompress
