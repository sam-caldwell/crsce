/**
 * @file verify_cross_sums.h
 * @brief Declaration for cross-sum verification helpers for reconstructed CSMs.
 * © Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "decompress/Csm/Csm.h"

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
    bool verify_cross_sums(const Csm &csm,
                           const std::array<std::uint16_t, Csm::kS> &lsm,
                           const std::array<std::uint16_t, Csm::kS> &vsm,
                           const std::array<std::uint16_t, Csm::kS> &dsm,
                           const std::array<std::uint16_t, Csm::kS> &xsm);
} // namespace crsce::decompress
