/**
 * @file BlockSolverStatus_get.cpp
 * @brief Implementation of get_block_solve_snapshot().
 * @author Sam Caldwell
 * © Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Block/detail/get_block_solve_snapshot.h"
#include "decompress/Block/detail/BlockSolverStatus_state.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

#include <optional>

namespace crsce::decompress {

    /**
     * @name get_block_solve_snapshot
     * @brief Retrieve the latest block solver snapshot if available.
     * @return std::optional<BlockSolveSnapshot>
     */
    std::optional<BlockSolveSnapshot> get_block_solve_snapshot() { // NOLINT(misc-use-internal-linkage)
        return crsce::decompress::detail::g_last_snapshot;
    }
}
