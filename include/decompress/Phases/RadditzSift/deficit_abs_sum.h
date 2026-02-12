/**
 * @file deficit_abs_sum.h
 * @brief Declaration of deficit_abs_sum helper for Radditz Sift.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <vector>
#include <cstddef>

namespace crsce::decompress::phases::detail {
    /** @brief Sum absolute values of deficits. */
    std::size_t deficit_abs_sum(const std::vector<int> &d);
}
