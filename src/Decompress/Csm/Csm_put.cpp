/**
 * @file Csm_put.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Csm/Csm.h"
#include "decompress/Exceptions/WriteFailureOnLockedCsmElement.h"
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace crsce::decompress {
    /**
     * @brief Implementation detail.
     */
    void Csm::put(std::size_t r, std::size_t c, bool v) {
        if (!in_bounds(r, c)) {
            throw std::out_of_range("Csm::put: index out of bounds");
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
