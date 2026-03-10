/**
 * @file FailedLiteralProber_tryProbeValue.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Core single-probe operation: assign, propagate, hash-check, undo.
 */
#include "decompress/Solvers/FailedLiteralProber.h"

#include <cstdint>

namespace crsce::decompress::solvers {

    /**
     * @name tryProbeValue
     * @brief Tentatively assign value v at (r, c), propagate, hash-check completed rows, then undo.
     *
     * Steps:
     *   1. Save undo point
     *   2. Assign (r, c) = v and record on undo stack
     *   3. Propagate via tryPropagateCell — if infeasible, undo and return false
     *   4. Record forced assignments on undo stack
     *   5. Hash-check the directly assigned row (if completed) and any rows completed by forcing
     *   6. Undo all assignments back to save point
     *   7. Return feasibility result
     *
     * @param r Row index.
     * @param c Column index.
     * @param v Value to probe (0 or 1).
     * @return True if the value is feasible; false if it leads to contradiction.
     * @throws None
     */
    bool FailedLiteralProber::tryProbeValue(const std::uint16_t r,
                                            const std::uint16_t c,
                                            const std::uint8_t v) {
        // 1. Save undo point
        const auto token = brancher_.saveUndoPoint();

        // 2. Assign and record
        store_.assign(r, c, v);
        brancher_.recordAssignment(r, c);

        // 3. Propagate
        const bool feasible = propagator_.tryPropagateCell(r, c);
        if (!feasible) {
            brancher_.undoToSavePoint(token);
            return false;
        }

        // 4. Record forced assignments on undo stack
        const auto &forced = propagator_.getForcedAssignments();
        for (const auto &a : forced) {
            brancher_.recordAssignment(a.r, a.c);
        }

        // 5. Hash-check completed rows
        bool hashOk = true;

        // Check the directly assigned cell's row
        if (store_.getStatDirect(r).unknown == 0) {
            if (!hasher_.verifyRow(r, store_.getRow(r))) {
                hashOk = false;
            }
        }

        // Check rows completed by forced propagation
        if (hashOk) {
            for (const auto &a : forced) {
                if (a.r != r && store_.getStatDirect(a.r).unknown == 0) {
                    if (!hasher_.verifyRow(a.r, store_.getRow(a.r))) {
                        hashOk = false;
                        break;
                    }
                }
            }
        }

        // 6. Undo all speculative assignments
        brancher_.undoToSavePoint(token);

        // 7. Return result
        return hashOk;
    }

} // namespace crsce::decompress::solvers
