/**
 * @file FailedLiteralProber.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Failed literal detection (probing) for strengthening constraint inference.
 */
#pragma once

#include <cstdint>

#include "decompress/Solvers/BranchingController.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/IHashVerifier.h"
#include "decompress/Solvers/PropagationEngine.h"

namespace crsce::decompress::solvers {

    /**
     * @struct ProbeResult
     * @name ProbeResult
     * @brief Result of probing both values for a single cell.
     */
    struct ProbeResult {
        /**
         * @name row
         * @brief Row index of the probed cell.
         */
        std::uint16_t row;

        /**
         * @name col
         * @brief Column index of the probed cell.
         */
        std::uint16_t col;

        /**
         * @name forcedValue
         * @brief Forced value: 0, 1, or 255 (no forcing — both values feasible).
         */
        std::uint8_t forcedValue;

        /**
         * @name bothInfeasible
         * @brief True if neither value is feasible (contradiction detected).
         */
        bool bothInfeasible;
    };

    /**
     * @class FailedLiteralProber
     * @name FailedLiteralProber
     * @brief Strengthens inference by tentatively assigning values, propagating, and detecting contradictions.
     *
     * If assigning value v to a cell leads to a contradiction (infeasible propagation or hash
     * mismatch), the opposite value is forced — eliminating a DFS branch entirely. Applied to
     * fixpoint before the DFS starts (probeToFixpoint) and per-node during the DFS (probeAlternate).
     */
    class FailedLiteralProber {
    public:
        /**
         * @name kS
         * @brief Matrix dimension.
         */
        static constexpr std::uint16_t kS = 511;

        /**
         * @name FailedLiteralProber
         * @brief Construct a prober bound to solver components.
         * @param store Reference to the constraint store (must outlive this prober).
         * @param propagator Reference to the propagation engine (must outlive this prober).
         * @param brancher Reference to the branching controller (must outlive this prober).
         * @param hasher Reference to the hash verifier (must outlive this prober).
         * @throws None
         */
        FailedLiteralProber(ConstraintStore &store,
                            PropagationEngine &propagator,
                            BranchingController &brancher,
                            IHashVerifier &hasher);

        /**
         * @name probeCell
         * @brief Probe both values for a single unassigned cell.
         *
         * Tentatively assigns 0 and then 1 to the cell, propagating each and checking
         * for contradictions (infeasible propagation or hash mismatch on completed rows).
         * All speculative assignments are undone before returning.
         *
         * @param r Row index.
         * @param c Column index.
         * @return ProbeResult indicating whether a value is forced or both are (in)feasible.
         * @throws None
         */
        [[nodiscard]] ProbeResult probeCell(std::uint16_t r, std::uint16_t c);

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
        [[nodiscard]] std::int32_t probeToFixpoint();

        /**
         * @name probeAlternate
         * @brief Check if a single value is feasible for a cell (used during DFS).
         *
         * Tentatively assigns the given value, propagates, and checks feasibility
         * including hash verification on completed rows. Undoes the assignment before returning.
         *
         * @param r Row index.
         * @param c Column index.
         * @param v Value to probe (0 or 1).
         * @return True if the value is feasible; false if it leads to contradiction.
         * @throws None
         */
        [[nodiscard]] bool probeAlternate(std::uint16_t r, std::uint16_t c, std::uint8_t v);

    private:
        /**
         * @name tryProbeValue
         * @brief Core single-probe operation: assign, propagate, hash-check, undo.
         * @param r Row index.
         * @param c Column index.
         * @param v Value to probe (0 or 1).
         * @return True if the value is feasible; false if it leads to contradiction.
         * @throws None
         */
        [[nodiscard]] bool tryProbeValue(std::uint16_t r, std::uint16_t c, std::uint8_t v);

        /**
         * @name store_
         * @brief Reference to the constraint store.
         */
        ConstraintStore &store_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

        /**
         * @name propagator_
         * @brief Reference to the propagation engine.
         */
        PropagationEngine &propagator_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

        /**
         * @name brancher_
         * @brief Reference to the branching controller.
         */
        BranchingController &brancher_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

        /**
         * @name hasher_
         * @brief Reference to the hash verifier.
         */
        IHashVerifier &hasher_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    };

} // namespace crsce::decompress::solvers
