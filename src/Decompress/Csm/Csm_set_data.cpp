/**
 * @file Csm_set_data.cpp
 * @brief Implementation of Csm::set_data.
 */
#include "decompress/Csm/detail/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"
#include <cstddef>

namespace crsce::decompress {
    void Csm::set_data(const std::size_t r, const std::size_t c, const double value, const bool lock_cell, bool /*async*/) {
        if (!in_bounds(r, c)) { throw CsmIndexOutOfBounds(r, c, kS); }
        auto &cell = cells_.at(r).at(c);
        if (lock_cell) {
            const CellMuGuard g(cell);
            data_.at(r).at(c) = value;
        } else {
            data_.at(r).at(c) = value;
        }
    }
} // namespace crsce::decompress
