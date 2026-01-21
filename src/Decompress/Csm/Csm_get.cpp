/**
 * @file Csm_get.cpp
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>
#include <stdexcept>

namespace crsce::decompress {
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
