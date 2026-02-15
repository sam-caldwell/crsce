/**
 * @file Csm_resolve_dx.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name resolve_dx
     * @brief resolve a cell at (d,x)
     * @param d diagonal coordinate
     * @param x antidiagonal coordinate
     * @return void
     */
    void Csm::resolve_dx(const std::size_t d, const std::size_t x) const {
        bounds_check(d, x);
        auto *cell = dx_cells_.at(d).at(x);
        cell->set_resolved(true);
    }
}
