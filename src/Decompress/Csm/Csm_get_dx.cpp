/**
 * @file Csm_get_dx.cpp
 * @author Sam Caldwell
 * @brief get_dx definition
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name get_dx
     * @brief returns the csm cell value at (d,x)
     * @param d diagonal coordinate
     * @param x anti-diagonal coordinate
     * @return cell value (bool)
     */
    bool Csm::get_dx(const std::size_t d, const std::size_t x) const {
        bounds_check(d, x);
        const auto *cell = dx_cells_.at(d).at(x);
        cell->wait_on_lock();
        return cell->data();
    }
}
