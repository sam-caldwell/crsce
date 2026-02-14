/**
 * @file Csm_get.cpp
 * @brief Implementation
 */
#include "decompress/Csm/detail/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"
#include <cstddef>

namespace crsce::decompress {
    bool Csm::get_rc(const std::size_t r, const std::size_t c) const {
        if (!in_bounds(r, c)) { throw CsmIndexOutOfBounds(r, c, kS); }
        return cells_.at(r).at(c).data();
    }
} // namespace crsce::decompress
