/**
 * @file Csm_put.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Csm/detail/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"
#include "common/exceptions/WriteFailureOnLockedCsmElement.h"
#include <cstddef>
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name Csm::put
     * @brief Store bit value at (r,c), rejecting writes to locked cells.
     * @param r Row index.
     * @param c Column index.
     * @param v Bit value to store.
     * @return void
     */
    void Csm::put(const std::size_t r, const std::size_t c, const bool v) {
        if (!in_bounds(r, c)) {
            throw CsmIndexOutOfBounds(r, c, kS);
        }
        const auto idx = index_of(r, c);
        const auto bidx = byte_index(idx);
        if (locks_[idx] != 0U) {
            throw WriteFailureOnLockedCsmElement("Csm::put: write to locked element");
        }
        const auto mask = bit_mask(idx);
        if (v) {
            bits_[bidx] = static_cast<std::uint8_t>(bits_[bidx] | mask);
        } else {
            bits_[bidx] = static_cast<std::uint8_t>(bits_[bidx] & static_cast<std::uint8_t>(~mask));
        }
    }
} // namespace crsce::decompress
