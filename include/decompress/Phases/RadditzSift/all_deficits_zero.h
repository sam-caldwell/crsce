/**
 * @file all_deficits_zero.h
 * @brief Declaration of all_deficits_zero helper for Radditz Sift.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <vector>

namespace crsce::decompress::phases::detail {
    /** @brief Return true if all deficits are zero. */
    bool all_deficits_zero(const std::vector<int> &deficit);
}
