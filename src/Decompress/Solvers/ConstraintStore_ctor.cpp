/**
 * @file ConstraintStore_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore constructor implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "decompress/Solvers/CellState.h"

namespace crsce::decompress::solvers {
    /**
     * @name ConstraintStore
     * @brief Construct a constraint store with target sums for all line families.
     * @param rowSums Target row sums (LSM), size s.
     * @param colSums Target column sums (VSM), size s.
     * @param diagSums Target diagonal sums (DSM), size 2s-1.
     * @param antiDiagSums Target anti-diagonal sums (XSM), size 2s-1.
     * @param slope256Sums Target slope-256 (HSM1) sums, size s.
     * @param slope255Sums Target slope-255 (SFC1) sums, size s.
     * @param slope2Sums Target slope-2 (HSM2) sums, size s.
     * @param slope509Sums Target slope-509 (SFC2) sums, size s.
     * @throws None
     */
    ConstraintStore::ConstraintStore(const std::vector<std::uint16_t> &rowSums,
                                     const std::vector<std::uint16_t> &colSums,
                                     const std::vector<std::uint16_t> &diagSums,
                                     const std::vector<std::uint16_t> &antiDiagSums,
                                     const std::vector<std::uint16_t> &slope256Sums,
                                     const std::vector<std::uint16_t> &slope255Sums,
                                     const std::vector<std::uint16_t> &slope2Sums,
                                     const std::vector<std::uint16_t> &slope509Sums)
        : cells_(static_cast<std::size_t>(kS) * kS, CellState::Unassigned),
          rowBits_(kS, std::array<std::uint64_t, 8>{}) {

        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)

        // Initialize row stats: stats_[0..kS)
        for (std::uint16_t r = 0; r < kS; ++r) {
            stats_[r].target = rowSums[r];
            stats_[r].unknown = kS;
            stats_[r].assigned = 0;
        }

        // Initialize column stats: stats_[kS..2*kS)
        for (std::uint16_t c = 0; c < kS; ++c) {
            stats_[kS + c].target = colSums[c];
            stats_[kS + c].unknown = kS;
            stats_[kS + c].assigned = 0;
        }

        // Initialize diagonal stats: stats_[2*kS..2*kS+kNumDiags)
        for (std::uint16_t d = 0; d < kNumDiags; ++d) {
            const auto idx = (2U * kS) + d;
            stats_[idx].target = diagSums[d];
            const auto len = static_cast<std::uint16_t>(
                std::min({static_cast<int>(d + 1), static_cast<int>(kS), static_cast<int>(kNumDiags - d)}));
            stats_[idx].unknown = len;
            stats_[idx].assigned = 0;
        }

        // Initialize anti-diagonal stats: stats_[2*kS+kNumDiags..kBasicLines)
        for (std::uint16_t x = 0; x < kNumAntiDiags; ++x) {
            const auto idx = (2U * kS) + kNumDiags + x;
            stats_[idx].target = antiDiagSums[x];
            const auto len = static_cast<std::uint16_t>(
                std::min({static_cast<int>(x + 1), static_cast<int>(kS), static_cast<int>(kNumAntiDiags - x)}));
            stats_[idx].unknown = len;
            stats_[idx].assigned = 0;
        }

        // Initialize 4 slope partition stats
        const std::array<const std::vector<std::uint16_t> *, kNumSlopePartitions> slopeSums = {
            &slope256Sums, &slope255Sums, &slope2Sums, &slope509Sums
        };
        for (std::uint16_t p = 0; p < kNumSlopePartitions; ++p) {
            const auto base = kSlopeBases[p];
            const auto &sums = *slopeSums[p];
            for (std::uint16_t k = 0; k < kNumSlope; ++k) {
                stats_[base + k].target = sums[k];
                stats_[base + k].unknown = kS; // toroidal lines always have s cells
                stats_[base + k].assigned = 0;
            }
        }

        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
    }
} // namespace crsce::decompress::solvers
