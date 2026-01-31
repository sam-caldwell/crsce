/**
 * @file BlockSolverStatus.cpp
 * @brief Implementation of shared block solver snapshot accessors.
 */
#include "decompress/Block/detail/BlockSolverStatus.h"
#include <optional>

namespace crsce::decompress {
    namespace {
        thread_local std::optional<BlockSolveSnapshot> g_last_snapshot; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    }

    void set_block_solve_snapshot(const BlockSolveSnapshot &s) {
        g_last_snapshot = s;
    }

    std::optional<BlockSolveSnapshot> get_block_solve_snapshot() {
        return g_last_snapshot;
    }

    void clear_block_solve_snapshot() {
        g_last_snapshot.reset();
    }
}
