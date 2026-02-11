/**
 * @file Detail_compute_deficits.cpp
 * @brief compute_deficits for Radditz helpers.
 * @author Sam Caldwell
 */
#include "decompress/Phases/RadditzSift/Detail.h"

namespace crsce::decompress::phases::detail {
    std::vector<int> compute_deficits(const std::vector<int> &col_count,
                                      std::span<const std::uint16_t> vsm) {
        const std::size_t S = col_count.size();
        std::vector<int> deficit(S, 0);
        for (std::size_t c = 0; c < S; ++c) {
            const int target = (c < vsm.size() ? static_cast<int>(vsm[c]) : 0);
            deficit[c] = target - col_count[c];
        }
        return deficit;
    }
}

