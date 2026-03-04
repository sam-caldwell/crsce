/**
 * @file FailedLiteralProber_probeCell.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief FailedLiteralProber::probeCell — probe both values for a single cell.
 */
#include "decompress/Solvers/FailedLiteralProber.h"

#include <cstdint>

namespace crsce::decompress::solvers {

    /**
     * @name probeCell
     * @brief Probe both values (0 and 1) for a single unassigned cell.
     *
     * Calls tryProbeValue for each value. If only one value is feasible, that value
     * is forced. If neither is feasible, the cell is contradictory. If both are
     * feasible, no forcing is possible.
     *
     * @param r Row index.
     * @param c Column index.
     * @return ProbeResult with the forced value or bothInfeasible flag.
     * @throws None
     */
    ProbeResult FailedLiteralProber::probeCell(const std::uint16_t r, const std::uint16_t c) {
        const bool zeroOk = tryProbeValue(r, c, 0);
        const bool oneOk = tryProbeValue(r, c, 1);

        if (!zeroOk && !oneOk) {
            return {.row = r, .col = c, .forcedValue = 255, .bothInfeasible = true};
        }
        if (!zeroOk) {
            return {.row = r, .col = c, .forcedValue = 1, .bothInfeasible = false};
        }
        if (!oneOk) {
            return {.row = r, .col = c, .forcedValue = 0, .bothInfeasible = false};
        }
        return {.row = r, .col = c, .forcedValue = 255, .bothInfeasible = false};
    }

} // namespace crsce::decompress::solvers
