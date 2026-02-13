/**
 * @file BlockSolverStatus_state.h
 * @brief Declaration of thread-local storage for the last block solver snapshot.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <optional>
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    /**
     * @name g_last_snapshot
     * @brief Thread-local store for the most recent BlockSolveSnapshot.
     */
    /**
     * @name g_last_snapshot
     * @brief The most recent BlockSolveSnapshot shared across threads.
     */
    extern std::optional<BlockSolveSnapshot> g_last_snapshot; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}
