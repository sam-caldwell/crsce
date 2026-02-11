/**
 * @file init_block_solve_snapshot.h
 * @brief Initialize BlockSolveSnapshot from current state.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <span>
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/DeterministicElimination/ConstraintState.h"

namespace crsce::decompress::detail {
    /**
     * @name init_block_solve_snapshot
     * @brief Populate snapshot fields from the current constraint state and vectors.
     * @param snap Snapshot to write.
     * @param S Matrix size (Csm::kS).
     * @param st Current constraint state.
     * @param lsm Row targets (span over decoded 9-bit values).
     * @param vsm Column targets.
     * @param dsm Diagonal targets.
     * @param xsm Anti-diagonal targets.
     * @param belief_seed RNG seed used for belief jitter.
     */
    void init_block_solve_snapshot(BlockSolveSnapshot &snap,
                                   std::size_t S,
                                   const ConstraintState &st,
                                   std::span<const std::uint16_t> lsm,
                                   std::span<const std::uint16_t> vsm,
                                   std::span<const std::uint16_t> dsm,
                                   std::span<const std::uint16_t> xsm,
                                   std::uint64_t belief_seed);
}
