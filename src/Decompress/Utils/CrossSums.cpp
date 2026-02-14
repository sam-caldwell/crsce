/**
 * @file CrossSums.cpp
 * @brief Implementation for cross-sum verification helpers for reconstructed CSMs.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Utils/detail/verify_cross_sums.h"
#include "decompress/Csm/detail/Csm.h" // direct provider for Csm
#include <array>    // direct provider for std::array
#include <cstddef>  // direct provider for std::size_t
#include <cstdint>  // direct provider for std::uint16_t

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
    bool verify_cross_sums(const Csm &csm,
                           const std::array<std::uint16_t, Csm::kS> &lsm,
                           const std::array<std::uint16_t, Csm::kS> &vsm,
                           const std::array<std::uint16_t, Csm::kS> &dsm,
                           const std::array<std::uint16_t, Csm::kS> &xsm) {
        constexpr std::size_t S = Csm::kS;
        for (std::size_t i = 0; i < S; ++i) {
            if (csm.count_lsm(i) != lsm.at(i)) { return false; }
            if (csm.count_vsm(i) != vsm.at(i)) { return false; }
            if (csm.count_dsm(i) != dsm.at(i)) { return false; }
            if (csm.count_xsm(i) != xsm.at(i)) { return false; }
        }
        return true;
    }
} // namespace crsce::decompress
