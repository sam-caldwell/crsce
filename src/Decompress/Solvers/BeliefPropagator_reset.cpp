/**
 * @file BeliefPropagator_reset.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief BeliefPropagator::reset — zero all messages for a cold restart.
 */
#include "decompress/Solvers/BeliefPropagator.h"

#include <algorithm>

namespace crsce::decompress::solvers {

    /**
     * @name reset
     * @brief Fill msg_ and msgSum_ with 0.0f, restoring uniform prior beliefs.
     *        After reset, belief(r, c) returns 0.5 for all cells.
     */
    void BeliefPropagator::reset() {
        std::ranges::fill(msg_,     0.0F);
        std::ranges::fill(msgSum_,  0.0F);
    }

} // namespace crsce::decompress::solvers
