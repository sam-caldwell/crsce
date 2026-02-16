/**
 * @file Csm_set_belief_dx.cpp
 * @author Sam Caldwell
 * @brief Set gobp belief via (d,x) addressing.
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include "decompress/Utils/detail/calc_c_from_d.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name set_belief_dx
     * @brief Set gobp belief value via (d,x) addressing.
     * @param d Diagonal coordinate.
     * @param x Anti-diagonal coordinate.
     * @param value Belief value.
     * @param lock Take MU lock during write if true.
     * @param async Unused placeholder for async behavior.
     * @return void
     */
    void Csm::set_belief_dx(const std::size_t d,
                            const std::size_t x,
                            const double value,
                            const bool lock,
                            const bool async) {

        bounds_check(d, x);
        const std::size_t r = dx_row_.at(d).at(x);
        const std::size_t c = ::crsce::decompress::detail::calc_c_from_d(r, d);
        auto *cell = dx_cells_.at(d).at(x);
        (void)async; // unused
        if (lock) {
            const CellMuGuard g(*cell);
            data_.at(r).at(c) = value;
        } else {
            data_.at(r).at(c) = value;
        }

    }
} // namespace crsce::decompress
