/**
 * @file BlockSolveSnapshot_ctor.cpp
 * @brief Constructor implementation for BlockSolveSnapshot to seed initial snapshot state.
 */
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include <span>
#include <cstdint>
#include <cstddef>

namespace crsce::decompress {
    BlockSolveSnapshot::BlockSolveSnapshot(std::size_t S_,
                                           const ConstraintState &st,
                                           std::span<const std::uint16_t> lsm_in,
                                           std::span<const std::uint16_t> vsm_in,
                                           std::span<const std::uint16_t> dsm_in,
                                           std::span<const std::uint16_t> xsm_in,
                                           std::uint64_t belief_seed)
        : S(S_), rng_seed_belief(belief_seed) {
        message.clear();

        // Copy targets
        lsm.assign(lsm_in.begin(), lsm_in.end());
        vsm.assign(vsm_in.begin(), vsm_in.end());
        dsm.assign(dsm_in.begin(), dsm_in.end());
        xsm.assign(xsm_in.begin(), xsm_in.end());

        // Seed unknown counts from constraint state
        U_row.assign(st.U_row.begin(), st.U_row.end());
        U_col.assign(st.U_col.begin(), st.U_col.end());
        U_diag.assign(st.U_diag.begin(), st.U_diag.end());
        U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());

        // Compute unknown_total and solved
        std::size_t sumU = 0;
        for (const auto u : st.U_row) {
            sumU += static_cast<std::size_t>(u);
        }
        unknown_total = sumU;
        solved = (S * S) - sumU;
    }
}
