/**
 * @file init_block_solve_snapshot.cpp
 */
#include "decompress/Block/detail/init_block_solve_snapshot.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"

namespace crsce::decompress::detail {
    void init_block_solve_snapshot(BlockSolveSnapshot &snap,
                                   const std::size_t S,
                                   const ConstraintState &st,
                                   std::span<const std::uint16_t> lsm,
                                   std::span<const std::uint16_t> vsm,
                                   std::span<const std::uint16_t> dsm,
                                   std::span<const std::uint16_t> xsm,
                                   const std::uint64_t belief_seed) {
        snap.S = S;
        snap.iter = 0;
        snap.phase = BlockSolveSnapshot::Phase::init;
        snap.message.clear();
        snap.lsm.assign(lsm.begin(), lsm.end());
        snap.vsm.assign(vsm.begin(), vsm.end());
        snap.dsm.assign(dsm.begin(), dsm.end());
        snap.xsm.assign(xsm.begin(), xsm.end());
        snap.U_row.assign(st.U_row.begin(), st.U_row.end());
        snap.U_col.assign(st.U_col.begin(), st.U_col.end());
        snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
        snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
        std::size_t sumU = 0;
        for (const auto u : st.U_row) { sumU += static_cast<std::size_t>(u); }
        snap.unknown_total = sumU;
        snap.solved = (S * S) - sumU;
        snap.rng_seed_belief = belief_seed;
    }
}
