/**
 * @file verify_cross_sums.h
 * @brief Declaration for cross-sum verification helpers for reconstructed CSMs.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "decompress/Csm/Csm.h"
#include "decompress/CrossSum/CrossSums.h"

namespace crsce::decompress {
    /**
     * @name verify_cross_sums
     * @brief Compute row/col/diag/xdiag counts and compare to expected vectors.
     * @param csm Cross-Sum Matrix to examine.
     * @param lsm Expected row counts (length S).
     * @param vsm Expected column counts (length S).
     * @param dsm Expected diagonal counts (length S).
     * @param xsm Expected anti-diagonal counts (length S).
     * @return bool True if computed counts match expected vectors.
     */
    bool verify_cross_sums(const Csm &csm, const CrossSums &sums);
} // namespace crsce::decompress
