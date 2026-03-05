/**
 * @file BeliefPropagator_belief.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief BeliefPropagator::belief — return sigmoid(msgSum) for a cell.
 */
#include "decompress/Solvers/BeliefPropagator.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace crsce::decompress::solvers {

    /**
     * @name belief
     * @brief P(cell(r,c) = 1) = sigmoid(Σ_t λ_{line_t → (r,c)}).
     *        Returns 0.5 when all messages are zero (initial or assigned state).
     * @param r Row index.
     * @param c Column index.
     * @return Marginal belief in (0, 1).
     */
    float BeliefPropagator::belief(const std::uint16_t r, const std::uint16_t c) const {
        const float lambda = msgSum_[(static_cast<std::size_t>(r) * kS) + c]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        return 1.0F / (1.0F + std::exp(-lambda));
    }

} // namespace crsce::decompress::solvers
