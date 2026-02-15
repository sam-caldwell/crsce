/**
 * @file Csm_is_locked_dx.cpp
 * @author Sam Caldwell
 * @brief implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name is_locked_dx
     * @brief returns whether a cell is locked using (d,x) space
     * @param d diagonal coordinate
     * @param x anti-diagonal coordinate
     * @return boolean value representing cell state
     */
    bool Csm::is_locked_dx(const std::size_t d, const std::size_t x) const {
        bounds_check(d, x);
        return dx_cells_.at(d).at(x)->resolved();
    }
}
