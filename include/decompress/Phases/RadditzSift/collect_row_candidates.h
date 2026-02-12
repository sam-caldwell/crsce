/**
 * @file collect_row_candidates.h
 * @brief Declaration of collect_row_candidates helper for Radditz Sift.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <vector>
#include <cstddef>
#include "decompress/Csm/detail/Csm.h"

namespace crsce::decompress::phases::detail {
    /** @brief Collect donor/target columns for a row given deficits. */
    void collect_row_candidates(const Csm &csm,
                                const std::vector<int> &deficit,
                                std::size_t r,
                                std::vector<std::size_t> &from,
                                std::vector<std::size_t> &to);
}
