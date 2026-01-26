/**
 * @file BlockSolver.h
 * @brief High-level block solver: reconstruct CSM from LH and cross-sum payloads.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "decompress/Block/detail/solve_block.h"

namespace crsce::decompress {
    /**
     * @name BlockSolverTag
     * @brief Tag type to satisfy one-definition-per-header for block solver declarations.
     */
    struct BlockSolverTag {
    };
}
