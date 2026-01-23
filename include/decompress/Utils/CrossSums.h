/**
 * @file CrossSums.h
 * @brief Cross-sum verification helpers for reconstructed CSMs.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <algorithm>

#include "decompress/Csm/Csm.h"

namespace crsce::decompress {
    /**
     * @brief Compute row/col/diag/xdiag counts and compare to expected vectors.
     */
    inline bool verify_cross_sums(const Csm &csm,
                                  const std::array<std::uint16_t, Csm::kS> &lsm,
                                  const std::array<std::uint16_t, Csm::kS> &vsm,
                                  const std::array<std::uint16_t, Csm::kS> &dsm,
                                  const std::array<std::uint16_t, Csm::kS> &xsm) {
        constexpr std::size_t S = Csm::kS;
        auto diag_index = [](const std::size_t r, const std::size_t c) -> std::size_t { return (r + c) % S; };
        auto xdiag_index = [](const std::size_t r, const std::size_t c) -> std::size_t {
            return (r >= c) ? (r - c) : (r + S - c);
        };

        std::array<std::uint16_t, S> row{};
        std::array<std::uint16_t, S> col{};
        std::array<std::uint16_t, S> diag{};
        std::array<std::uint16_t, S> xdg{};

        for (std::size_t r = 0; r < S; ++r) {
            for (std::size_t c = 0; c < S; ++c) {
                if (const bool bit = csm.get(r, c); !bit) { continue; }
                row.at(r) = static_cast<std::uint16_t>(row.at(r) + 1U);
                col.at(c) = static_cast<std::uint16_t>(col.at(c) + 1U);
                diag.at(diag_index(r, c)) = static_cast<std::uint16_t>(diag.at(diag_index(r, c)) + 1U);
                xdg.at(xdiag_index(r, c)) = static_cast<std::uint16_t>(xdg.at(xdiag_index(r, c)) + 1U);
            }
        }
        return std::equal(row.begin(), row.end(), lsm.begin()) &&
               std::equal(col.begin(), col.end(), vsm.begin()) &&
               std::equal(diag.begin(), diag.end(), dsm.begin()) &&
               std::equal(xdg.begin(), xdg.end(), xsm.begin());
    }
} // namespace crsce::decompress
