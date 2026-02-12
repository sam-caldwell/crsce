/**
 * @file compute_col_counts.h
 * @brief Declaration of compute_col_counts helper for Radditz Sift.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <vector>
#include "decompress/Csm/detail/Csm.h"

namespace crsce::decompress::phases::detail {
    /** @brief Count ones in each column of the CSM. */
    std::vector<int> compute_col_counts(const Csm &csm);
}
