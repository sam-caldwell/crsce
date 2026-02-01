/**
 * @file BlockSolverStatus_clear.cpp
 * @brief Implementation of clear_block_solve_snapshot().
 * @author Sam Caldwell
 * © Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Block/detail/clear_block_solve_snapshot.h"
#include "decompress/Block/detail/BlockSolverStatus_state.h"

#include <optional>

namespace crsce::decompress {

    /**
     * @name clear_block_solve_snapshot
     * @brief Clear the stored block solver snapshot.
     * @return void
     */
    void clear_block_solve_snapshot() { // NOLINT(misc-use-internal-linkage)
        crsce::decompress::detail::g_last_snapshot.reset();
    }
}
