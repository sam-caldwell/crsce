/**
 * @file Detail_greedy_pair_row.cpp
 * @brief greedy_pair_row for Radditz helpers.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <vector>
#include <cstddef>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Phases/RadditzSift/greedy_pair_row.h"
#include "decompress/Phases/RadditzSift/swap_lateral.h"

namespace crsce::decompress::phases::detail {
    /**
     * @name greedy_pair_row
     * @brief Greedily pair donor and target columns on a row, updating deficits and metrics.
     * @param csm Matrix to modify (lateral swaps only).
     * @param r Row index.
     * @param from Donor columns (1s) for this row.
     * @param to Target columns (0s) for this row.
     * @param deficit Deficits per column; gets updated in-place.
     * @param swaps Swap counter to increment.
     * @param snap Snapshot to update partial adoption metric.
     * @return bool True if at least one swap occurred on this row; otherwise false.
     */
    bool greedy_pair_row(Csm &csm,
                         const std::size_t r,
                         std::vector<std::size_t> &from,
                         std::vector<std::size_t> &to,
                         std::vector<int> &deficit,
                         std::size_t &swaps,
                         BlockSolveSnapshot &snap) {
        bool row_improved = false;
        std::size_t i = 0;
        std::size_t j = 0;
        while (i < from.size() && j < to.size()) {
            const std::size_t cf = from[i];
            const std::size_t ct = to[j];
            if (swap_lateral(csm, r, cf, ct)) {
                deficit[cf] += 1; // reducing surplus at cf
                deficit[ct] -= 1; // filling deficit at ct
                ++swaps;
                ++snap.partial_adoptions;
                row_improved = true;
            }
            ++i; ++j; // consume each pair at most once
        }
        return row_improved;
    }
}
