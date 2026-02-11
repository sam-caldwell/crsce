/**
 * @file Detail_greedy_pair_row.cpp
 * @brief greedy_pair_row for Radditz helpers.
 * @author Sam Caldwell
 */
#include "decompress/Phases/RadditzSift/Detail.h"

namespace crsce::decompress::phases::detail {
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

