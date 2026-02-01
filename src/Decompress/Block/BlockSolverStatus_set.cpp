/**
 * @file BlockSolverStatus_set.cpp
 * @brief Implementation of set_block_solve_snapshot().
 * @author Sam Caldwell
  * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "decompress/Block/detail/BlockSolverStatus_state.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

#include <optional>

namespace crsce::decompress {

    /**
     * @name set_block_solve_snapshot
     * @brief Store the latest block solver snapshot (thread-local).
     * @param s Snapshot to store.
     * @return void
     */
    void set_block_solve_snapshot(const BlockSolveSnapshot &s) { // NOLINT(misc-use-internal-linkage)
        crsce::decompress::detail::g_last_snapshot = s;
    }
}
