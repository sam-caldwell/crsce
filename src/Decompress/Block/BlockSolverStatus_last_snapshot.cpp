/**
 * @file BlockSolverStatus_last_snapshot.cpp
 * @brief Define storage for the last block solver snapshot value.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Block/detail/BlockSolverStatus_state.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

#include <optional>

namespace crsce::decompress::detail {
    /**
     * @name g_last_snapshot
     * @brief The most recent BlockSolveSnapshot shared across threads.
     *        Guarded externally by g_last_snapshot_mu.
     */
    std::optional<BlockSolveSnapshot> g_last_snapshot; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}
