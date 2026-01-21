/**
 * @file Csm_is_locked.cpp
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>
#include <stdexcept>

namespace crsce::decompress {
    bool Csm::is_locked(std::size_t r, std::size_t c) const {
        if (!in_bounds(r, c)) {
            throw std::out_of_range("Csm::is_locked: index out of bounds");
        }
        return locks_[index_of(r, c)] != 0U;
    }
} // namespace crsce::decompress
