/**
 * @file all_deficits_zero.h
 * @author Sam Caldwell
 * @brief Declaration of all_deficits_zero helper for Radditz Sift.
 */
#pragma once

#include <vector>

namespace crsce::decompress::phases::detail {
    /** @brief Return true if all deficits are zero. */
    bool all_deficits_zero(const std::vector<int> &deficit);
}

