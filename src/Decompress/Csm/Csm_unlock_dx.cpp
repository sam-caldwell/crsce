/**
 * @file Csm_unlock_dx.cpp
 * @author Sam Caldwell
 * @brief Release MU (mutex) for a cell addressed in (d,x) space.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name unlock_dx
     * @brief Release the per-cell MU guard for the element at (d,x).
     * @param d diagonal coordinate
     * @param x anti-diagonal coordinate
     */
    void Csm::unlock_dx(const std::size_t d, const std::size_t x) {
        bounds_check(d, x);
        auto *cell = dx_cells_.at(d).at(x);
        cell->unlock_mu();
    }
}

