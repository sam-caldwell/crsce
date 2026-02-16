/**
 * @file CrossSums.cpp
 * @brief Implementation for cross-sum verification helpers for reconstructed CSMs.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Utils/detail/verify_cross_sums.h"
#include "decompress/Csm/Csm.h"
#include "decompress/CrossSum/CrossSum.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name verify_cross_sums
     * @brief Recompute cross-sums from CSM and compare against provided sums.
     * @param csm Reconstructed Cross-Sum Matrix.
     * @param lsm Left-to-right row sums.
     * @param vsm Top-to-bottom column sums.
     * @param dsm Diagonal sums: d = (c - r) mod S.
     * @param xsm Anti-diagonal sums: x = (r + c) mod S.
     * @return bool True if all four families match; false otherwise.
     */
    bool verify_cross_sums(const Csm &csm, const CrossSums &sums) {
        constexpr std::size_t S = Csm::kS;
        for (std::size_t i = 0; i < S; ++i) {
            if (csm.count_lsm(i) != sums.lsm().targets().at(i)) { return false; }
            if (csm.count_vsm(i) != sums.vsm().targets().at(i)) { return false; }
            if (csm.count_dsm(i) != sums.dsm().targets().at(i)) { return false; }
            if (csm.count_xsm(i) != sums.xsm().targets().at(i)) { return false; }
        }
        return true;
    }
} // namespace crsce::decompress
