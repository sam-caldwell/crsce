/**
 * @file Detail_compute_col_counts.cpp
 * @brief compute_col_counts for Radditz helpers.
 * @author Sam Caldwell
 */
#include "decompress/Phases/RadditzSift/Detail.h"

namespace crsce::decompress::phases::detail {
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

