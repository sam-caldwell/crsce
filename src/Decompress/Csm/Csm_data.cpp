/**
 * @file Csm_data.cpp
 * @author Sam Caldwell
 * @brief Read cell belief value (waits briefly if MU-locked).
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>

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
        const auto &cell = cells_.at(r).at(c);
        cell.wait_on_lock();
        return data_.at(r).at(c);
    }
} // namespace crsce::decompress
