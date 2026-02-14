/**
 * @file Csm_lock.cpp
 * @brief Implementation
 */
#include "decompress/Csm/detail/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"
#include <cstddef>

namespace crsce::decompress {
    void Csm::lock_rc(const std::size_t r, const std::size_t c, const bool lock_val) {
        if (!in_bounds(r, c)) { throw CsmIndexOutOfBounds(r, c, kS); }
        auto &cell = cells_.at(r).at(c);
        if (lock_val) { cell.set_locked(true); } else { cell.set_locked(false); }
    }
} // namespace crsce::decompress
