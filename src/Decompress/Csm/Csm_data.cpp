/**
 * @file Csm_data.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>
#include <stdexcept>

namespace crsce::decompress {
    /**
     * @brief Implementation detail.
     */
    void Csm::set_data(std::size_t r, std::size_t c, double value) {
        if (!in_bounds(r, c)) {
            throw std::out_of_range("Csm::set_data: index out of bounds");
        }
        data_[index_of(r, c)] = value;
    }

    double Csm::get_data(std::size_t r, std::size_t c) const {
        if (!in_bounds(r, c)) {
            throw std::out_of_range("Csm::get_data: index out of bounds");
        }
        return data_[index_of(r, c)];
    }
} // namespace crsce::decompress
