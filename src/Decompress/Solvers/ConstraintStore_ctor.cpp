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
#include <cstdlib>
#include <fstream>
#include <ios>
#include <vector>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/LtpTable.h"

namespace crsce::decompress::solvers {
    /**
     * @name ConstraintStore
     * @brief Construct a constraint store with target sums for all line families.
     * @param rowSums Target row sums (LSM), size s.
     * @param colSums Target column sums (VSM), size s.
     * @param diagSums Target diagonal sums (DSM), size 2s-1.
     * @param antiDiagSums Target anti-diagonal sums (XSM), size 2s-1.
     * @param ltp1Sums Target LTP1 partition sums, size s.
     * @param ltp2Sums Target LTP2 partition sums, size s.
     * @param ltp3Sums Target LTP3 partition sums, size s.
     * @param ltp4Sums Target LTP4 partition sums, size s.
     * @param ltp5Sums Target LTP5 partition sums, size s.
     * @param ltp6Sums Target LTP6 partition sums, size s.
     * @throws None
     */
    ConstraintStore::ConstraintStore(const std::vector<std::uint16_t> &rowSums,
                                     const std::vector<std::uint16_t> &colSums,
                                     const std::vector<std::uint16_t> &diagSums,
                                     const std::vector<std::uint16_t> &antiDiagSums,
                                     const std::vector<std::uint16_t> &ltp1Sums,
                                     const std::vector<std::uint16_t> &ltp2Sums,
                                     const std::vector<std::uint16_t> &ltp3Sums,
                                     const std::vector<std::uint16_t> &ltp4Sums,
                                     const std::vector<std::uint16_t> &ltp5Sums,
                                     const std::vector<std::uint16_t> &ltp6Sums)
        : cells_(static_cast<std::size_t>(kS) * kS, CellState::Unassigned),
          rowBits_(kS, std::array<std::uint64_t, 2>{}) {

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

        // Initialize LTP1 partition stats: stats_[kLTP1Base .. kLTP1Base + kS)
        // B.23: each line k has ltp_len(k) = kLtpS = 511 cells (uniform)
        for (std::uint16_t k = 0; k < kS; ++k) {
            stats_[kLTP1Base + k].target = ltp1Sums[k];
            stats_[kLTP1Base + k].unknown = static_cast<std::uint16_t>(ltpLineLen(k));
            stats_[kLTP1Base + k].assigned = 0;
        }

        // Initialize LTP2 partition stats: stats_[kLTP2Base .. kLTP2Base + kS)
        for (std::uint16_t k = 0; k < kS; ++k) {
            stats_[kLTP2Base + k].target = ltp2Sums[k];
            stats_[kLTP2Base + k].unknown = static_cast<std::uint16_t>(ltpLineLen(k));
            stats_[kLTP2Base + k].assigned = 0;
        }

        // B.57: only 2 LTP sub-tables. LTP3-6 parameters are unused.
        (void)ltp3Sums; (void)ltp4Sums; (void)ltp5Sums; (void)ltp6Sums;

        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
    }
} // namespace crsce::decompress::solvers
