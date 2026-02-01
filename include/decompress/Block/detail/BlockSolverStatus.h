/**
 * @file BlockSolverStatus.h
 * @brief Shared snapshot/report state for a single block solve attempt.
 * @author Sam Caldwell
 * © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

// Convenience umbrella header to include the BlockSolveSnapshot and snapshot accessors.
// Intentionally contains no constructs to satisfy OneDefinitionPerHeader plugin.

#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "decompress/Block/detail/get_block_solve_snapshot.h"
#include "decompress/Block/detail/clear_block_solve_snapshot.h"
