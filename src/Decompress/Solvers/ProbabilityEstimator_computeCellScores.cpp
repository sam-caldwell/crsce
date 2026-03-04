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
     * For each unassigned cell, reads the 7 non-row line statistics (col, diag,
     * anti-diag, 4 slopes) to compute score1 (P(1) proxy) and score0 (P(0) proxy).
     * Products are bounded by 511^7 < 2^63, fitting uint64_t. Sorts by confidence
     * (|score1 - score0|) descending so most-constrained cells branch first.
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

            // Flat stat indices for the 3 original non-row lines
            const auto ci = static_cast<std::uint32_t>(kS) + c;
            const auto di = (2U * kS) + static_cast<std::uint32_t>(c - r + (kS - 1));
            const auto xi = (2U * kS) + kNumDiags + static_cast<std::uint32_t>(r + c);

            // Precomputed flat indices for the 4 slope lines (eliminates expensive % 511)
            const auto &sl = ConstraintStore::slopeFlatIndices(r, c);
            const auto si0 = static_cast<std::uint32_t>(sl[0]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            const auto si1 = static_cast<std::uint32_t>(sl[1]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            const auto si2 = static_cast<std::uint32_t>(sl[2]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            const auto si3 = static_cast<std::uint32_t>(sl[3]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

            // Read all 7 non-row line stats
            const auto &colStat = store_.getStatDirect(ci);
            const auto &diagStat = store_.getStatDirect(di);
            const auto &antiStat = store_.getStatDirect(xi);
            const auto &s0Stat = store_.getStatDirect(si0);
            const auto &s1Stat = store_.getStatDirect(si1);
            const auto &s2Stat = store_.getStatDirect(si2);
            const auto &s3Stat = store_.getStatDirect(si3);

            // rho = target - assigned (remaining ones needed)
            const auto rhoCol = static_cast<std::uint64_t>(colStat.target - colStat.assigned);
            const auto rhoDiag = static_cast<std::uint64_t>(diagStat.target - diagStat.assigned);
            const auto rhoAnti = static_cast<std::uint64_t>(antiStat.target - antiStat.assigned);
            const auto rhoS0 = static_cast<std::uint64_t>(s0Stat.target - s0Stat.assigned);
            const auto rhoS1 = static_cast<std::uint64_t>(s1Stat.target - s1Stat.assigned);
            const auto rhoS2 = static_cast<std::uint64_t>(s2Stat.target - s2Stat.assigned);
            const auto rhoS3 = static_cast<std::uint64_t>(s3Stat.target - s3Stat.assigned);

            // unknowns remaining on each line
            const auto uCol = static_cast<std::uint64_t>(colStat.unknown);
            const auto uDiag = static_cast<std::uint64_t>(diagStat.unknown);
            const auto uAnti = static_cast<std::uint64_t>(antiStat.unknown);
            const auto uS0 = static_cast<std::uint64_t>(s0Stat.unknown);
            const auto uS1 = static_cast<std::uint64_t>(s1Stat.unknown);
            const auto uS2 = static_cast<std::uint64_t>(s2Stat.unknown);
            const auto uS3 = static_cast<std::uint64_t>(s3Stat.unknown);

            // score1 = product of 7 rho values (proportional to P(1))
            const std::uint64_t s1 = rhoCol * rhoDiag * rhoAnti * rhoS0 * rhoS1 * rhoS2 * rhoS3;

            // score0 = product of 7 (u - rho) values
            const std::uint64_t s0 = (uCol - rhoCol) * (uDiag - rhoDiag) * (uAnti - rhoAnti)
                                   * (uS0 - rhoS0) * (uS1 - rhoS1) * (uS2 - rhoS2) * (uS3 - rhoS3);

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
