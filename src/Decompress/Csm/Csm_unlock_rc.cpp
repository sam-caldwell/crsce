/**
 * @file Csm_unlock_rc.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name unlock_rc
     * @brief unlocks a CSM cell in (r,c) coordinate space
     * @param r row
     * @param c column
     * @return void
     */
    void Csm::unlock_rc(const std::size_t r, const std::size_t c) {
        bounds_check(r, c);
        auto &cell = cells_.at(r).at(c);
        cell.unlock_mu();
    }
}
