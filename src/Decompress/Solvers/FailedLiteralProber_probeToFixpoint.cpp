/**
 * @file FailedLiteralProber_probeToFixpoint.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief FailedLiteralProber::probeToFixpoint — probe all unassigned cells to fixpoint.
 */
#include "decompress/Solvers/FailedLiteralProber.h"

#include <cstdint>
#include <string>

#include "common/O11y/O11y.h"
#include "decompress/Solvers/CellState.h"

namespace crsce::decompress::solvers {

    /**
     * @name probeToFixpoint
     * @brief Probe all unassigned cells repeatedly until no new forcings are discovered.
     *
     * In each pass, iterates all unassigned cells in row-major order and probes each.
     * When a value is forced, it is permanently assigned and propagated. Repeats until
     * a full pass produces zero new forcings (fixpoint) or a contradiction is detected.
     *
     * @return Number of cells forced (>= 0), or -1 if a contradiction was detected.
     * @throws None
     */
    std::int32_t FailedLiteralProber::probeToFixpoint() {
        std::int32_t totalForced = 0;
        std::uint32_t round = 0;
        bool madeProgress = true;

        while (madeProgress) {
            ++round;
            madeProgress = false;
            std::uint32_t passForced = 0;
            std::uint32_t probedCells = 0;

            for (std::uint16_t r = 0; r < kS; ++r) {
                // Skip rows with no unassigned cells
                if (store_.getStatDirect(r).unknown == 0) {
                    continue;
                }

                for (std::uint16_t c = 0; c < kS; ++c) {
                    if (store_.getCellState(r, c) != CellState::Unassigned) {
                        continue;
                    }

                    ++probedCells;
                    const auto result = probeCell(r, c);

                    if (result.bothInfeasible) {
                        ::crsce::o11y::O11y::instance().event("prober_contradiction",
                            {{"round", std::to_string(round)},
                             {"row", std::to_string(r)},
                             {"col", std::to_string(c)}});
                        return -1;
                    }

                    if (result.forcedValue != 255) {
                        // Permanently assign the forced value
                        store_.assign(r, c, result.forcedValue);
                        brancher_.recordAssignment(r, c);

                        // Propagate the forced assignment
                        const bool feasible = propagator_.tryPropagateCell(r, c);
                        if (!feasible) {
                            ::crsce::o11y::O11y::instance().event("prober_contradiction",
                                {{"round", std::to_string(round)},
                                 {"row", std::to_string(r)},
                                 {"col", std::to_string(c)}});
                            return -1;
                        }

                        // Record forced assignments from propagation on undo stack
                        const auto &forced = propagator_.getForcedAssignments();
                        for (const auto &a : forced) {
                            brancher_.recordAssignment(a.r, a.c);
                        }

                        ++passForced;
                        ++totalForced;
                        madeProgress = true;
                    }
                }
            }

            ::crsce::o11y::O11y::instance().event("prober_pass",
                {{"round", std::to_string(round)},
                 {"probed_cells", std::to_string(probedCells)},
                 {"forced_cells", std::to_string(passForced)}});
        }

        // Count remaining unassigned cells
        std::uint32_t remaining = 0;
        for (std::uint16_t r = 0; r < kS; ++r) {
            remaining += store_.getStatDirect(r).unknown;
        }

        ::crsce::o11y::O11y::instance().event("prober_fixpoint",
            {{"total_rounds", std::to_string(round)},
             {"total_forced", std::to_string(totalForced)},
             {"remaining_unassigned", std::to_string(remaining)}});

        return totalForced;
    }

} // namespace crsce::decompress::solvers
