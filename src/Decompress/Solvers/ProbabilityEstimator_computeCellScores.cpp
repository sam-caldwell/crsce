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

namespace crsce::decompress::solvers {

    /**
     * @name computeCellScores
     * @brief Compute probability-guided scores for all unassigned cells in row r.
     *
     * For each unassigned cell, reads column/diagonal/anti-diagonal residuals and
     * unknowns to compute score1 (P(1) proxy) and score0 (P(0) proxy). Products
     * are bounded by 511^3 = 133,432,831 < 2^27, fitting uint32_t. Sorts by
     * confidence (|score1 - score0|) descending so most-constrained cells branch first.
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

            // Flat stat indices for the 3 non-row lines
            const auto ci = static_cast<std::uint32_t>(kS) + c;
            const auto di = (2U * kS) + static_cast<std::uint32_t>(c - r + (kS - 1));
            const auto xi = (2U * kS) + kNumDiags + static_cast<std::uint32_t>(r + c);

            // Read line stats
            const auto &colStat = store_.getStatDirect(ci);
            const auto &diagStat = store_.getStatDirect(di);
            const auto &antiStat = store_.getStatDirect(xi);

            // rho = target - assigned (remaining ones needed)
            const auto rhoCol = static_cast<std::uint32_t>(colStat.target - colStat.assigned);
            const auto rhoDiag = static_cast<std::uint32_t>(diagStat.target - diagStat.assigned);
            const auto rhoAnti = static_cast<std::uint32_t>(antiStat.target - antiStat.assigned);

            // unknowns remaining on each line
            const auto uCol = static_cast<std::uint32_t>(colStat.unknown);
            const auto uDiag = static_cast<std::uint32_t>(diagStat.unknown);
            const auto uAnti = static_cast<std::uint32_t>(antiStat.unknown);

            // score1 = rho_col * rho_diag * rho_anti (proportional to P(1))
            const std::uint32_t s1 = rhoCol * rhoDiag * rhoAnti;

            // score0 = (u_col - rho_col) * (u_diag - rho_diag) * (u_anti - rho_anti)
            const std::uint32_t s0 = (uCol - rhoCol) * (uDiag - rhoDiag) * (uAnti - rhoAnti);

            const auto preferred = static_cast<std::uint8_t>(s1 > s0 ? 1 : 0);
            const std::uint32_t confidence = (s1 > s0) ? (s1 - s0) : (s0 - s1);

            scores.push_back({r, c, s1, s0, preferred, confidence});
        }

        // Sort by confidence descending (most-constrained cells first)
        std::ranges::sort(scores, [](const CellScore &a, const CellScore &b) {
            return a.confidence > b.confidence;
        });

        return scores;
    }

} // namespace crsce::decompress::solvers
