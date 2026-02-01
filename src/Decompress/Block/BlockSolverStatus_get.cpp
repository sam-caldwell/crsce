/**
 * @file BlockSolverStatus_get.cpp
 * @brief Implementation of get_block_solve_snapshot().
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
     * @name get_block_solve_snapshot
     * @brief Retrieve the latest block solver snapshot if available.
     * @return std::optional<BlockSolveSnapshot>
     */
    std::optional<BlockSolveSnapshot> get_block_solve_snapshot() {
        return g_last_snapshot;
    }
}

