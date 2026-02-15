/**
 * @file Csm_bounds_check.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name bounds_check
     * @brief Private bounds_check helper for rc/dx indices
     * @param r row
     * @param c column
     * @return void
     */
    void Csm::bounds_check(const std::size_t r, const std::size_t c) const {
        if (!in_bounds(r, c)) {
            throw CsmIndexOutOfBounds(r, c, kS);
        }
    }
}
