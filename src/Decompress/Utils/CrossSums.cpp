/**
 * @file CrossSums.cpp
 * @brief Implementation for cross-sum verification helpers for reconstructed CSMs.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Utils/detail/verify_cross_sums.h"
#include "decompress/Csm/detail/Csm.h" // direct provider for Csm
#include <array>    // direct provider for std::array
#include <cstddef>  // direct provider for std::size_t
#include <cstdint>  // direct provider for std::uint16_t
#include <algorithm>

namespace crsce::decompress {
    /**
     * @name verify_cross_sums
     * @brief Recompute cross-sums from CSM and compare against provided sums.
     * @param csm Reconstructed Cross-Sum Matrix.
     * @param lsm Left-to-right row sums.
     * @param vsm Top-to-bottom column sums.
     * @param dsm Diagonal sums (r+c mod S).
     * @param xsm Anti-diagonal sums (r>=c ? r-c : r+S-c).
     * @return bool True if all four families match; false otherwise.
     */
    bool verify_cross_sums(const Csm &csm,
                           const std::array<std::uint16_t, Csm::kS> &lsm,
                           const std::array<std::uint16_t, Csm::kS> &vsm,
                           const std::array<std::uint16_t, Csm::kS> &dsm,
                           const std::array<std::uint16_t, Csm::kS> &xsm) {
        constexpr std::size_t S = Csm::kS;
        std::array<std::uint16_t, S> row{};
        std::array<std::uint16_t, S> col{};
        std::array<std::uint16_t, S> diag{};
        std::array<std::uint16_t, S> xdg{};

        for (std::size_t r = 0; r < S; ++r) {
            for (std::size_t c = 0; c < S; ++c) {
                if (const bool bit = csm.get(r, c); !bit) { continue; }
                row.at(r) = static_cast<std::uint16_t>(row.at(r) + 1U);
                col.at(c) = static_cast<std::uint16_t>(col.at(c) + 1U);
                const std::size_t d = (r + c) % S;
                const std::size_t x = (r >= c) ? (r - c) : (r + S - c);
                diag.at(d) = static_cast<std::uint16_t>(diag.at(d) + 1U);
                xdg.at(x) = static_cast<std::uint16_t>(xdg.at(x) + 1U);
            }
        }
        return std::equal(row.begin(), row.end(), lsm.begin()) &&
               std::equal(col.begin(), col.end(), vsm.begin()) &&
               std::equal(diag.begin(), diag.end(), dsm.begin()) &&
               std::equal(xdg.begin(), xdg.end(), xsm.begin());
    }
} // namespace crsce::decompress
