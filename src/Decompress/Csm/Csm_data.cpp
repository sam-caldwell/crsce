/**
 * @file Csm_data.cpp
 * @brief Implementation
 */
#include "decompress/Csm/detail/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"
#include <cstddef>

namespace crsce::decompress {
    double Csm::get_data(const std::size_t r, const std::size_t c) const {
        if (!in_bounds(r, c)) { throw CsmIndexOutOfBounds(r, c, kS); }
        return data_.at(r).at(c);
    }
} // namespace crsce::decompress
