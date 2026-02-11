/**
 * @file compute_col_counts.h
 * @author Sam Caldwell
 * @brief Declaration of compute_col_counts helper for Radditz Sift.
 */
#pragma once

#include <vector>
#include "decompress/Csm/detail/Csm.h"

namespace crsce::decompress::phases::detail {
    /** @brief Count ones in each column of the CSM. */
    std::vector<int> compute_col_counts(const Csm &csm);
}

