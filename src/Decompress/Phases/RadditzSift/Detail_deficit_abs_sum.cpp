/**
 * @file Detail_deficit_abs_sum.cpp
 * @brief deficit_abs_sum for Radditz helpers.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <vector>
#include <cstddef>
#include "decompress/Phases/RadditzSift/deficit_abs_sum.h"

namespace crsce::decompress::phases::detail {
    /**
     * @name deficit_abs_sum
     * @brief Sum absolute values in the deficit vector (for telemetry).
     * @param d Deficits per column.
     * @return std::size_t Sum of |d[c]|.
     */
    std::size_t deficit_abs_sum(const std::vector<int> &d) {
        std::size_t acc = 0;
        for (const int v : d) { acc += static_cast<std::size_t>(v >= 0 ? v : -v); }
        return acc;
    }
}
