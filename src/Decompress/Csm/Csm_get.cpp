/**
 * @file Csm_get.cpp
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
    bool Csm::get(std::size_t r, std::size_t c) const {
        if (!in_bounds(r, c)) {
            throw std::out_of_range("Csm::get: index out of bounds");
        }
        const auto idx = index_of(r, c);
        const auto bidx = byte_index(idx);
        const auto mask = bit_mask(idx);
        return (bits_[bidx] & mask) != 0U;
    }
} // namespace crsce::decompress
