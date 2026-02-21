/**
 * @file SnapshotGuard.h
 * @brief RAII helper to publish BlockSolveSnapshot updates at scope boundaries.
 */
#pragma once

#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"

namespace crsce::decompress::detail {
    class SnapshotGuard {
    public:
        explicit SnapshotGuard(BlockSolveSnapshot &snap) noexcept : s_(snap) {
            set_block_solve_snapshot(s_);
        }
        ~SnapshotGuard() {
            // Best-effort; never throw from dtor
            try { set_block_solve_snapshot(s_); } catch (...) {}
        }
        SnapshotGuard(const SnapshotGuard&) = delete;
        SnapshotGuard &operator=(const SnapshotGuard&) = delete;
        SnapshotGuard(SnapshotGuard&&) = delete;
        SnapshotGuard &operator=(SnapshotGuard&&) = delete;

        void publish() const { set_block_solve_snapshot(s_); }

    private:
        BlockSolveSnapshot &s_;
    };
}

