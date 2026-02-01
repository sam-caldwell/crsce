/**
 * @file BlockSolverStatus_clear.cpp
 * @brief Implementation of clear_block_solve_snapshot().
 * @author Sam Caldwell
 * © Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Block/detail/BlockSolverStatus.h"

#include <optional>

namespace crsce::decompress {
    namespace {
        extern thread_local std::optional<BlockSolveSnapshot> g_last_snapshot; // NOLINT
    }

    /**
     * @name clear_block_solve_snapshot
     * @brief Clear the stored block solver snapshot.
     * @return void
     */
    void clear_block_solve_snapshot() {
        g_last_snapshot.reset();
    }
}

