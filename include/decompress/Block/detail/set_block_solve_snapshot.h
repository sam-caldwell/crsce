/**
 * @file set_block_solve_snapshot.h
 * @brief Declaration of set_block_solve_snapshot().
 * @author Sam Caldwell
 * © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <optional>
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress {
    /**
     * @name set_block_solve_snapshot
     * @brief Store the latest block solver snapshot (thread-local).
     * @param s Snapshot to store.
     * @return void
     */
    void set_block_solve_snapshot(const BlockSolveSnapshot &s);
}

