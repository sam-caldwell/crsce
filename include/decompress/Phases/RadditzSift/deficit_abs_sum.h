/**
 * @file deficit_abs_sum.h
 * @author Sam Caldwell
 * @brief Declaration of deficit_abs_sum helper for Radditz Sift.
 */
#pragma once

#include <vector>
#include <cstddef>

namespace crsce::decompress::phases::detail {
    /** @brief Sum absolute values of deficits. */
    std::size_t deficit_abs_sum(const std::vector<int> &d);
}

