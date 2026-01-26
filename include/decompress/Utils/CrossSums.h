/**
 * @file CrossSums.h
 * @brief Cross-sum verification helpers for reconstructed CSMs.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include "decompress/Utils/detail/verify_cross_sums.h"

namespace crsce::decompress {
    /**
     * @name CrossSumsTag
     * @brief Tag type for cross-sum utilities (aggregator header).
     */
    struct CrossSumsTag {
    };
} // namespace crsce::decompress
