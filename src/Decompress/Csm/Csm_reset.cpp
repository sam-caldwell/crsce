/**
 * @file Csm_reset.cpp
 * @brief Implementation
 * @author Sam Caldwell
 * @copyright (c) 2026 Sam Caldwell. See License.txt for details.
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>
#include <atomic>

namespace crsce::decompress {
    /**
     * @name reset
     * @brief reset the CSM state
     * @return void
     */
    void Csm::reset() {
        // Zero storage
        for (std::size_t r = 0; r < kS; ++r) {
            for (std::size_t c = 0; c < kS; ++c) {
                cells_.at(r).at(c).clear();
                data_.at(r).at(c) = 0.0;
            }
            row_versions_.at(r).store(0ULL, std::memory_order_relaxed);
            lsm_c_.at(r) = 0;
            vsm_c_.at(r) = 0;
            dsm_c_.at(r) = 0;
            xsm_c_.at(r) = 0;
        }
        // Create the (d,x) coordinate translation table
        build_dx_tables();
    }
} // namespace crsce::decompress
