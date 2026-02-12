/**
 * @file Detail_compute_col_counts.cpp
 * @brief compute_col_counts for Radditz helpers.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <vector>
#include <cstddef>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/Phases/RadditzSift/compute_col_counts.h"

namespace crsce::decompress::phases::detail {
    /**
     * @name compute_col_counts
     * @brief Count ones per column across the matrix.
     * @param csm Matrix to count.
     * @return std::vector<int> 1-counts per column (size Csm::kS).
     */
    std::vector<int> compute_col_counts(const Csm &csm) {
        const std::size_t S = Csm::kS;
        std::vector<int> col_count(S, 0);
        for (std::size_t c = 0; c < S; ++c) {
            int cnt = 0;
            for (std::size_t r = 0; r < S; ++r) { if (csm.get(r, c)) { ++cnt; } }
            col_count[c] = cnt;
        }
        return col_count;
    }
}
