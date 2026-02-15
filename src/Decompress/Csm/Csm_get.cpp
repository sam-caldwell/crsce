/**
 * @file Csm_get.cpp
 * @author Sam Caldwell
 * @brief Implement get_rc() to return cell value at CSM cell (r,c)
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name get_rc
     * @brief Using (r,c) space, return the CSM cell value.
     * @param r row
     * @param c column
     * @return boolean cell value (set/clear)
     */
    bool Csm::get_rc(const std::size_t r, const std::size_t c) const {
        if (!in_bounds(r, c)) { throw CsmIndexOutOfBounds(r, c, kS); }
        return cells_.at(r).at(c).data();
    }
} // namespace crsce::decompress
