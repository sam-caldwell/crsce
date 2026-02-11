/**
 * @file DeterministicElimination_force_xdiag.cpp
 * @brief Implementation of DeterministicElimination::force_xdiag (phase).
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Utils/detail/calc_c_from_x.h"
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
        // XSM[x]: all cells where r + c ≡ x (mod S) -> c = (x - r) mod S
        const auto to_assign = static_cast<std::size_t>(st_.U_xdiag.at(x));
        std::size_t assigned = 0;
        for (std::size_t r = 0; r < S; ++r) {
            if (assigned >= to_assign) { break; }
            const std::size_t c = ::crsce::decompress::detail::calc_c_from_x(r, x);
            if (csm_.is_locked(r, c)) { continue; }
            apply_cell(r, c, value);
            ++progress;
            ++assigned;
        }
    }
} // namespace crsce::decompress
