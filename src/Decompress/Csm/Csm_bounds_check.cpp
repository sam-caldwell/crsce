/**
 * @file Csm_bounds_check.cpp
 * @brief Private bounds_check helper for rc/dx indices
 */
#include "decompress/Csm/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"

namespace crsce::decompress {
    void Csm::bounds_check(const std::size_t r, const std::size_t c) const {
        if (!in_bounds(r, c)) {
            throw CsmIndexOutOfBounds(r, c, kS);
        }
    }
}

