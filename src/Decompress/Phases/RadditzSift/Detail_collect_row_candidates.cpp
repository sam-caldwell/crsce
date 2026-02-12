/**
 * @file Detail_collect_row_candidates.cpp
 * @brief collect_row_candidates for Radditz helpers.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <vector>
#include <cstddef>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/Phases/RadditzSift/collect_row_candidates.h"

namespace crsce::decompress::phases::detail {
    /**
     * @name collect_row_candidates
     * @brief For a given row, collect donor columns (surplus) and target columns (deficit).
     * @param csm Matrix to inspect (no locks moved here).
     * @param deficit Current column deficits (target - have).
     * @param r Row index.
     * @param from Output donor column indices where row has 1 and deficit[c] < 0.
     * @param to Output target column indices where row has 0 and deficit[c] > 0.
     * @return void
     */
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
