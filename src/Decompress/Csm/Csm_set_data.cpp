/**
 * @file Csm_set_data.cpp
 * @brief Implementation of Csm::set_data.
 * © Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Csm/detail/Csm.h"
#include <cstddef>
#include <stdexcept>

namespace crsce::decompress {
    /**
     * @name Csm::set_data
     * @brief Set auxiliary data value for (r,c).
     * @param r Row index.
     * @param c Column index.
     * @param value Data value to store.
     * @return void
     */
    void Csm::set_data(const std::size_t r, const std::size_t c, const double value) {
        if (!in_bounds(r, c)) {
            throw std::out_of_range("Csm::set_data: index out of bounds");
        }
        data_[index_of(r, c)] = value;
    }
} // namespace crsce::decompress
