/**
 * @file ProbabilityEstimator_computeCellScores.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ProbabilityEstimator::computeCellScores implementation.
 */
#include "decompress/Solvers/ProbabilityEstimator.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/LtpTable.h"

namespace crsce::decompress::solvers {

    /**
     * @name computeCellScores
     * @brief Compute probability-guided scores for all unassigned cells in row r.
     *
     * For each unassigned cell, reads the non-row line statistics (col, diag, anti-diag,
     * and 1-2 LTP lines per B.21) to compute score1 (P(1) proxy) and score0 (P(0) proxy).
     * Sorts by confidence (|score1 - score0|) descending so most-constrained cells branch first.
     *
     * @param r Row index in [0, kS).
     * @return Vector of CellScore sorted by confidence descending.
     * @throws None
     */
    auto ProbabilityEstimator::computeCellScores(const std::uint16_t r) const -> std::vector<CellScore> {
        static constexpr std::uint16_t kNumDiags = (2 * kS) - 1;

        std::vector<CellScore> scores;
        scores.reserve(kS);

        for (std::uint16_t c = 0; c < kS; ++c) {
            if (store_.getCellState(r, c) != CellState::Unassigned) {
                continue;
            }

            // Flat stat indices for the 3 basic non-row lines
            const auto ci = static_cast<std::uint32_t>(kS) + c;
            const auto di = (2U * kS) + static_cast<std::uint32_t>(c - r + (kS - 1));
            const auto xi = (2U * kS) + kNumDiags + static_cast<std::uint32_t>(r + c);

            // B.21: 1 or 2 LTP lines
            const auto &mem = ltpMembership(r, c);

            const auto &colStat  = store_.getStatDirect(ci);
            const auto &diagStat = store_.getStatDirect(di);
            const auto &antiStat = store_.getStatDirect(xi);
            const auto &l0Stat   = store_.getStatDirect(static_cast<std::uint32_t>(mem.flat[0])); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

            const auto rhoCol  = static_cast<std::uint64_t>(colStat.target  - colStat.assigned);
            const auto rhoDiag = static_cast<std::uint64_t>(diagStat.target - diagStat.assigned);
            const auto rhoAnti = static_cast<std::uint64_t>(antiStat.target - antiStat.assigned);
            const auto rhoL0   = static_cast<std::uint64_t>(l0Stat.target   - l0Stat.assigned);

            const auto uCol  = static_cast<std::uint64_t>(colStat.unknown);
            const auto uDiag = static_cast<std::uint64_t>(diagStat.unknown);
            const auto uAnti = static_cast<std::uint64_t>(antiStat.unknown);
            const auto uL0   = static_cast<std::uint64_t>(l0Stat.unknown);

            std::uint64_t s1 = rhoCol * rhoDiag * rhoAnti * rhoL0;
            std::uint64_t s0 = (uCol - rhoCol) * (uDiag - rhoDiag) * (uAnti - rhoAnti) * (uL0 - rhoL0);

            // If cell belongs to a second LTP sub-table, factor it in
            if (mem.count > 1) {
                const auto &l1Stat = store_.getStatDirect(static_cast<std::uint32_t>(mem.flat[1])); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                const auto rhoL1   = static_cast<std::uint64_t>(l1Stat.target - l1Stat.assigned);
                const auto uL1     = static_cast<std::uint64_t>(l1Stat.unknown);
                s1 *= rhoL1;
                s0 *= (uL1 - rhoL1);
            }

            const auto preferred = static_cast<std::uint8_t>(s1 > s0 ? 1 : 0);
            const std::uint64_t confidence = (s1 > s0) ? (s1 - s0) : (s0 - s1);

            scores.push_back({r, c, s1, s0, preferred, confidence});
        }

        // Sort by confidence descending (most-constrained cells first)
        std::ranges::sort(scores, [](const CellScore &a, const CellScore &b) {
            return a.confidence > b.confidence;
        });

        return scores;
    }

} // namespace crsce::decompress::solvers
