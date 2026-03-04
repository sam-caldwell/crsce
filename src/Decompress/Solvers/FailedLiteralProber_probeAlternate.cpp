/**
 * @file FailedLiteralProber_probeAlternate.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief FailedLiteralProber::probeAlternate — check if a single value is feasible.
 */
#include "decompress/Solvers/FailedLiteralProber.h"

#include <cstdint>

namespace crsce::decompress::solvers {

    /**
     * @name probeAlternate
     * @brief Check if assigning value v to cell (r, c) is feasible.
     *
     * Used during DFS to probe the alternate branch value. If the alternate is
     * infeasible, the current branch value can be taken without risk of needing
     * to backtrack to try the other.
     *
     * @param r Row index.
     * @param c Column index.
     * @param v Value to probe (0 or 1).
     * @return True if the value is feasible; false if it leads to contradiction.
     * @throws None
     */
    bool FailedLiteralProber::probeAlternate(const std::uint16_t r,
                                             const std::uint16_t c,
                                             const std::uint8_t v) {
        return tryProbeValue(r, c, v);
    }

} // namespace crsce::decompress::solvers
