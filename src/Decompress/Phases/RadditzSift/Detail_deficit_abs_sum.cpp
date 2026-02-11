/**
 * @file Detail_deficit_abs_sum.cpp
 * @brief deficit_abs_sum for Radditz helpers.
 * @author Sam Caldwell
 */
#include "decompress/Phases/RadditzSift/Detail.h"

namespace crsce::decompress::phases::detail {
    std::size_t deficit_abs_sum(const std::vector<int> &d) {
        std::size_t acc = 0;
        for (const int v : d) { acc += static_cast<std::size_t>(v >= 0 ? v : -v); }
        return acc;
    }
}

