/**
 * @file Csm_get.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Csm/detail/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name Csm::get
     * @brief Retrieve bit value at (r,c).
     * @param r Row index.
     * @param c Column index.
     * @return bool Bit value at the specified cell.
     */
    bool Csm::get(const std::size_t r, const std::size_t c) const {
        if (!in_bounds(r, c)) {
            throw CsmIndexOutOfBounds(r, c, kS);
        }
        const auto idx = index_of(r, c);
        const auto bidx = byte_index(idx);
        const auto mask = bit_mask(idx);
        return (bits_[bidx] & mask) != 0U;
    }
} // namespace crsce::decompress
