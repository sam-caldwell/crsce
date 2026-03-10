/**
 * @file FailedLiteralProber_probeAlternateDeep.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Recursive k-level exhaustive lookahead for deeper contradiction detection.
 */
#include "decompress/Solvers/FailedLiteralProber.h"

#include <cstdint>

namespace crsce::decompress::solvers {

    /**
     * @name probeAlternateDeep
     * @brief Recursive k-level lookahead: assign, propagate, hash-check, then recurse.
     *
     * Algorithm:
     *   1. Save undo point
     *   2. Assign (r, c) = v, record, propagate
     *   3. If propagation infeasible -> undo, return false
     *   4. Record forced assignments, hash-check completed rows
     *   5. If hash mismatch -> undo, return false
     *   6. If depth <= 1 -> undo, return true (leaf: feasible at this depth)
     *   7. Pick next unassigned cell via brancher_.nextCell()
     *   8. If no cell -> undo, return true (fully assigned = feasible)
     *   9. Recurse on both values at depth-1
     *  10. If BOTH infeasible -> undo, return false (contradiction)
     *  11. Undo, return true
     *
     * Recursion depth is bounded by k (max 4), worst case 2^k = 16 probe operations.
     *
     * @param r Row index.
     * @param c Column index.
     * @param v Value to probe (0 or 1).
     * @param depth Remaining probe depth.
     * @return True if feasible at this depth; false if contradiction detected.
     * @throws None
     */
    // NOLINTNEXTLINE(misc-no-recursion) — bounded recursion: depth <= kMaxK (4), safe for stack
    bool FailedLiteralProber::probeAlternateDeep(const std::uint16_t r,
                                                  const std::uint16_t c,
                                                  const std::uint8_t v,
                                                  const std::uint8_t depth) {
        // 1. Save undo point
        const auto token = brancher_.saveUndoPoint();

        // 2. Assign and record
        store_.assign(r, c, v);
        brancher_.recordAssignment(r, c);

        // 3. Propagate
        if (!propagator_.tryPropagateCell(r, c)) {
            brancher_.undoToSavePoint(token);
            return false;
        }

        // 4. Record forced assignments
        const auto &forced = propagator_.getForcedAssignments();
        for (const auto &a : forced) {
            brancher_.recordAssignment(a.r, a.c);
        }

        // 5. Hash-check completed rows
        if (store_.getStatDirect(r).unknown == 0) {
            if (!hasher_.verifyRow(r, store_.getRow(r))) {
                brancher_.undoToSavePoint(token);
                return false;
            }
        }
        for (const auto &a : forced) {
            if (a.r != r && store_.getStatDirect(a.r).unknown == 0) {
                if (!hasher_.verifyRow(a.r, store_.getRow(a.r))) {
                    brancher_.undoToSavePoint(token);
                    return false;
                }
            }
        }

        // 6. Leaf: feasible at this depth
        if (depth <= 1) {
            brancher_.undoToSavePoint(token);
            return true;
        }

        // 7. Pick next unassigned cell
        const auto next = brancher_.nextCell();
        if (!next.has_value()) {
            // Fully assigned = feasible
            brancher_.undoToSavePoint(token);
            return true;
        }

        const auto [nr, nc] = next.value();

        // 9. Recurse on both values at depth-1
        const bool zero_feasible = probeAlternateDeep(nr, nc, 0, depth - 1);
        const bool one_feasible = probeAlternateDeep(nr, nc, 1, depth - 1);

        // 10. If BOTH infeasible, this assignment leads to contradiction
        const bool feasible = zero_feasible || one_feasible;

        // 11. Undo and return
        brancher_.undoToSavePoint(token);
        return feasible;
    }

} // namespace crsce::decompress::solvers
