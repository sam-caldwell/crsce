/**
 * @file Csm_unlock_dx.cpp
 * @author Sam Caldwell
 * @brief Unlock MU for a dx-addressed cell.
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name unlock_dx
     * @brief Clear the MU lock at (d,x) address.
     * @param d Diagonal coordinate.
     * @param x Anti-diagonal coordinate.
     * @return void
     */
    void Csm::unlock_dx(const std::size_t d, const std::size_t x) {
        bounds_check(d, x);
        auto *cell = dx_cells_.at(d).at(x);
        cell->unlock_mu();
    }
}
