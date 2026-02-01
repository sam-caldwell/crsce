/**
 * @file BlockSolverStatus_set.cpp
 * @brief Implementation of set_block_solve_snapshot().
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Block/detail/BlockSolverStatus.h"

#include <optional>

namespace crsce::decompress {
    namespace {
        extern thread_local std::optional<BlockSolveSnapshot> g_last_snapshot; // NOLINT
    }

    /**
     * @name set_block_solve_snapshot
     * @brief Store the latest block solver snapshot (thread-local).
     * @param s Snapshot to store.
     * @return void
     */
    void set_block_solve_snapshot(const BlockSolveSnapshot &s) {
        g_last_snapshot = s;
    }
}

