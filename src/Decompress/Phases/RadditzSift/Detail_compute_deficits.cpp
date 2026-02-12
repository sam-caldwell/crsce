/**
 * @file Detail_compute_deficits.cpp
 * @brief compute_deficits for Radditz helpers.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <vector>
#include <span>
#include <cstdint>
#include <cstddef>
#include "decompress/Phases/RadditzSift/compute_deficits.h"

namespace crsce::decompress::phases::detail {
    /**
     * @name compute_deficits
     * @brief Compute per-column deficit = vsm[c] - have[c].
     * @param col_count Ones per column (have[c]).
     * @param vsm Target ones per column.
     * @return std::vector<int> Deficits per column (size = vsm size).
     */
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
