/**
 * @file get_block_solve_snapshot.h
 * @brief Declaration of get_block_solve_snapshot().
 * @author Sam Caldwell
 * © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <optional>
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress {
    /**
     * @name get_block_solve_snapshot
     * @brief Retrieve the latest block solver snapshot if available.
     * @return std::optional<BlockSolveSnapshot>
     */
    std::optional<BlockSolveSnapshot> get_block_solve_snapshot();
}

