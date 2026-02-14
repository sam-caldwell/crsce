/**
 * @file dsm_accept_swap.cpp
 * @brief DSM sift acceptance check on a proposed 2x2 swap.
 */

#include "decompress/Block/detail/dsm_accept_swap.h"
#include <cstddef>

#include "decompress/Block/detail/get_block_solve_snapshot.h"

namespace crsce::decompress::detail {
    bool dsm_accept_swap(const std::size_t dx_before, const std::size_t dx_after,
                         const int d_sat_lost) {
        if (dx_after > dx_before) { return false; }
        int lock_dsm = 0;
        if (const auto snap = crsce::decompress::get_block_solve_snapshot(); snap.has_value()) {
            lock_dsm = snap->lock_dsm_sat;
        }
        if (lock_dsm && d_sat_lost > 0) { return false; }
        return true;
    }
}
