/**
 * @file Csm_data.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Csm/detail/Csm.h"
#include <cstddef>
#include <stdexcept>

namespace crsce::decompress {
    /**
     * @name Csm::get_data
     * @brief Retrieve auxiliary data value for (r,c).
     * @param r Row index.
     * @param c Column index.
     * @return double Stored value.
     */
    double Csm::get_data(const std::size_t r, const std::size_t c) const {
        if (!in_bounds(r, c)) {
            throw std::out_of_range("Csm::get_data: index out of bounds");
        }
        return data_[index_of(r, c)];
    }
} // namespace crsce::decompress
