/**
 * @file BlockSolverStatus_last_snapshot.cpp
 * @brief Define thread-local storage for the last block solver snapshot.
 * @author Sam Caldwell
  * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Block/detail/BlockSolverStatus_state.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

#include <optional>

namespace crsce::decompress::detail {
    /**
     * @name g_last_snapshot
     * @brief Thread-local store for the most recent BlockSolveSnapshot.
     */
    thread_local std::optional<BlockSolveSnapshot> g_last_snapshot; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}
