/**
 * @file Csm_data.cpp
 * @author Sam Caldwell
 * @brief Csm method to return belief data.
 * @copyright (c) 2026 Sam Caldwell. See License.txt for details.
 */
#include "decompress/Csm/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"
#include <cstddef>
#include <thread>

namespace crsce::decompress {
    /**
     * @name get_data
     * @brief return the given cell's belief data
     * @param r row
     * @param c column
     * @return belief data (double)
     */
    double Csm::get_data(const std::size_t r, const std::size_t c) const {

        bounds_check(r, c);
        auto &cell = cells_.at(r).at(c);
        cell.wait_on_lock();
        return data_.at(r).at(c);
    }
} // namespace crsce::decompress
