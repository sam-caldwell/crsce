/**
 * @file Csm_is_locked_rc.cpp
 * @author Sam Caldwell
 * @brief implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name is_locked_rc
     * @brief returns whether csm cell is locked using (r,c) coordinates
     * @param r row
     * @param c columns
     * @return boolean cell state
     */
    bool Csm::is_locked_rc(const std::size_t r, const std::size_t c) const {
        bounds_check(r, c);
        return cells_.at(r).at(c).resolved();
    }
} // namespace crsce::decompress
