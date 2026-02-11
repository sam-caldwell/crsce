/**
 * @file Detail_collect_row_candidates.cpp
 * @brief collect_row_candidates for Radditz helpers.
 * @author Sam Caldwell
 */
#include "decompress/Phases/RadditzSift/Detail.h"

namespace crsce::decompress::phases::detail {
    void collect_row_candidates(const Csm &csm,
                                const std::vector<int> &deficit,
                                const std::size_t r,
                                std::vector<std::size_t> &from,
                                std::vector<std::size_t> &to) {
        const std::size_t S = Csm::kS;
        from.clear(); to.clear();
        for (std::size_t c = 0; c < S; ++c) {
            const bool bit = csm.get(r, c);
            if (bit) {
                if (deficit[c] < 0 && !csm.is_locked(r, c)) { from.push_back(c); }
            } else {
                if (deficit[c] > 0 && !csm.is_locked(r, c)) { to.push_back(c); }
            }
        }
    }
}

