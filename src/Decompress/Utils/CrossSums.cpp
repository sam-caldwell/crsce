/**
 * @file CrossSums.cpp
 * @brief Implementation for cross-sum verification helpers for reconstructed CSMs.
 */
#include "decompress/Utils/detail/verify_cross_sums.h"

#include <algorithm>

namespace crsce::decompress {
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

