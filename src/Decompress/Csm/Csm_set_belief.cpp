/**
 * @file Csm_set_belief.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name set_belief
     * @brief set a gobp belief value
     * @param r row
     * @param c column
     * @param value belief value
     * @param lock Take MU lock during write if true.
     * @param async Unused placeholder for async behavior.
     * @return void
     */
    void Csm::set_belief(const std::size_t r,
                         const std::size_t c,
                         const double value,
                         const bool lock, const bool async) {

        bounds_check(r, c);
        auto &cell = cells_.at(r).at(c);

        (void)async; // parameter presently unused
        if (lock) {
            const CellMuGuard g(cell);
            data_.at(r).at(c) = value;
        } else {
            data_.at(r).at(c) = value;
        }

    }
} // namespace crsce::decompress
