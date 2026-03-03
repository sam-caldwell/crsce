/**
 * @file ProbabilityEstimator_computeGlobalCellScores.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ProbabilityEstimator::computeGlobalCellScores implementation.
 */
#include "decompress/Solvers/ProbabilityEstimator.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/ConstraintStore.h"

namespace crsce::decompress::solvers {

    /**
     * @name computeGlobalCellScores
     * @brief Compute probability-guided scores for all unassigned cells across all rows.
     *
     * Iterates every row, scoring each unassigned cell using column, diagonal, and
     * anti-diagonal residuals. Products are bounded by 511^3 < 2^27, fitting uint32_t.
     * Returns all cells sorted by confidence descending so the global DFS assigns
     * most-constrained cells first.
     *
     * @return Vector of CellScore for all unassigned cells, sorted by confidence descending.
     * @throws None
     */
    auto ProbabilityEstimator::computeGlobalCellScores() const -> std::vector<CellScore> {
        static constexpr std::uint16_t kNumDiags = (2 * kS) - 1;
        static constexpr std::uint32_t kMaxCells = static_cast<std::uint32_t>(kS) * kS;

        std::vector<CellScore> scores;
        scores.reserve(kMaxCells);

        for (std::uint16_t r = 0; r < kS; ++r) {
            for (std::uint16_t c = 0; c < kS; ++c) {
                if (store_.getCellState(r, c) != CellState::Unassigned) {
                    continue;
                }

                // Flat stat indices for the 3 non-row lines
                const auto ci = static_cast<std::uint32_t>(kS) + c;
                const auto di = (2U * kS) + static_cast<std::uint32_t>(c - r + (kS - 1));
                const auto xi = (2U * kS) + kNumDiags + static_cast<std::uint32_t>(r + c);

                const auto &colStat = store_.getStatDirect(ci);
                const auto &diagStat = store_.getStatDirect(di);
                const auto &antiStat = store_.getStatDirect(xi);

                const auto rhoCol = static_cast<std::uint32_t>(colStat.target - colStat.assigned);
                const auto rhoDiag = static_cast<std::uint32_t>(diagStat.target - diagStat.assigned);
                const auto rhoAnti = static_cast<std::uint32_t>(antiStat.target - antiStat.assigned);

                const auto uCol = static_cast<std::uint32_t>(colStat.unknown);
                const auto uDiag = static_cast<std::uint32_t>(diagStat.unknown);
                const auto uAnti = static_cast<std::uint32_t>(antiStat.unknown);

                const std::uint32_t s1 = rhoCol * rhoDiag * rhoAnti;
                const std::uint32_t s0 = (uCol - rhoCol) * (uDiag - rhoDiag) * (uAnti - rhoAnti);

                const auto preferred = static_cast<std::uint8_t>(s1 > s0 ? 1 : 0);
                const std::uint32_t confidence = (s1 > s0) ? (s1 - s0) : (s0 - s1);

                scores.push_back({r, c, s1, s0, preferred, confidence});
            }
        }

        std::ranges::sort(scores, [](const CellScore &a, const CellScore &b) {
            return a.confidence > b.confidence;
        });

        return scores;
    }

} // namespace crsce::decompress::solvers
