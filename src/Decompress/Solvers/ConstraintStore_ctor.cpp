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
     * @throws None
     */
    ConstraintStore::ConstraintStore(const std::vector<std::uint16_t> &rowSums,
                                     const std::vector<std::uint16_t> &colSums,
                                     const std::vector<std::uint16_t> &diagSums,
                                     const std::vector<std::uint16_t> &antiDiagSums)
        : cells_(static_cast<std::size_t>(kS) * kS, CellState::Unassigned),
          rowStats_(kS),
          colStats_(kS),
          diagStats_(kNumDiags),
          antiDiagStats_(kNumAntiDiags),
          rowBits_(kS, std::array<std::uint64_t, 8>{}) {

        // Initialize row stats
        for (std::uint16_t r = 0; r < kS; ++r) {
            rowStats_[r].target = rowSums[r];
            rowStats_[r].unknown = kS;
            rowStats_[r].assigned = 0;
        }

        // Initialize column stats
        for (std::uint16_t c = 0; c < kS; ++c) {
            colStats_[c].target = colSums[c];
            colStats_[c].unknown = kS;
            colStats_[c].assigned = 0;
        }

        // Initialize diagonal stats
        for (std::uint16_t d = 0; d < kNumDiags; ++d) {
            diagStats_[d].target = diagSums[d];
            const auto len = static_cast<std::uint16_t>(
                std::min({static_cast<int>(d + 1), static_cast<int>(kS), static_cast<int>(kNumDiags - d)}));
            diagStats_[d].unknown = len;
            diagStats_[d].assigned = 0;
        }

        // Initialize anti-diagonal stats
        for (std::uint16_t x = 0; x < kNumAntiDiags; ++x) {
            antiDiagStats_[x].target = antiDiagSums[x];
            const auto len = static_cast<std::uint16_t>(
                std::min({static_cast<int>(x + 1), static_cast<int>(kS), static_cast<int>(kNumAntiDiags - x)}));
            antiDiagStats_[x].unknown = len;
            antiDiagStats_[x].assigned = 0;
        }
    }
} // namespace crsce::decompress::solvers
