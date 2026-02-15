/**
 * @file Csm_get.cpp
 * @author Sam Caldwell
 * @brief Implement get_rc() to return cell value at CSM cell (r,c)
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"
#include <cstddef>
#include <thread>

namespace crsce::decompress {
    /**
     * @name get_rc
     * @brief Using (r,c) space, return the CSM cell value.
     * @param r row
     * @param c column
     * @return boolean cell value (set/clear)
     */
    bool Csm::get_rc(const std::size_t r, const std::size_t c) const {
        bounds_check(r, c);
        auto &cell = cells_.at(r).at(c);
        cell.wait_on_lock();
        return cell.data();
    }
} // namespace crsce::decompress
