/**
 * @file DeterministicElimination_force_diag.cpp
 * @brief Implementation of DeterministicElimination::force_diag.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Utils/detail/index_calc.h"
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
        // DSM[d]: all cells where c = (r + d) mod S
        const auto to_assign = static_cast<std::size_t>(st_.U_diag.at(d));
        std::size_t assigned = 0;
        for (std::size_t r = 0; r < S; ++r) {
            if (assigned >= to_assign) { break; }
            const std::size_t c = ::crsce::decompress::detail::calc_c_from_d(r, d);
            if (csm_.is_locked(r, c)) { continue; }
            apply_cell(r, c, value);
            ++progress;
            ++assigned;
        }
    }
} // namespace crsce::decompress
