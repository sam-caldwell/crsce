/**
 * @file Csm_lock.cpp
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>
#include <stdexcept>

namespace crsce::decompress {
    void Csm::lock(std::size_t r, std::size_t c) {
        if (!in_bounds(r, c)) {
            throw std::out_of_range("Csm::lock: index out of bounds");
        }
        locks_[index_of(r, c)] = 1U;
    }
} // namespace crsce::decompress
