/**
 * @file ConstraintStore_snapshot.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief ConstraintStore snapshot/restore for row-level backtracking (B.59h).
 */
#include "decompress/Solvers/ConstraintStore.h"

namespace crsce::decompress::solvers {

    /**
     * @name takeSnapshot
     * @brief Save the complete mutable state.
     * @return Snapshot containing copies of all mutable members.
     * @throws None
     */
    auto ConstraintStore::takeSnapshot() const -> Snapshot {
        return {
            .cells = cells_,
            .stats = stats_,
            .rowBits = rowBits_,
            .assigned = assigned_,
        };
    }

    /**
     * @name restoreSnapshot
     * @brief Restore the mutable state from a snapshot.
     * @param snap The snapshot to restore.
     * @throws None
     */
    void ConstraintStore::restoreSnapshot(const Snapshot &snap) {
        cells_ = snap.cells;
        stats_ = snap.stats;
        rowBits_ = snap.rowBits;
        assigned_ = snap.assigned;
    }

} // namespace crsce::decompress::solvers
