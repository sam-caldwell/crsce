/**
 * @file Decompressor_solve_block.cpp
 * @brief Decompressor::solve_block implementation: orchestrates solver
 *        selection (via factory) and phases per block.
 */

#include "decompress/Decompressor/Decompressor.h"

#include <cstddef>
#include <cstdint>
#include <span>

#include "decompress/Csm/Csm.h"
#include "decompress/CrossSum/CrossSums.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "decompress/Block/detail/prelock_padded_tail.h"
#include "decompress/Block/detail/verify_cross_sums_and_lh.h"
#include "decompress/Phases/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/run_hybrid_sift.h"
#include "decompress/Solvers/SelectedSolver.h"
#include "decompress/Block/detail/seed_initial_beliefs.h"
#include "decompress/Block/detail/execute_bitsplash_and_validate.h"
#include "decompress/Block/detail/execute_radditz_and_validate.h"
#include "decompress/Block/detail/reseed_residuals_from_csm.h"
#include "decompress/HashMatrix/LateralHashMatrix.h"
#include "decompress/CrossSum/LateralSumMatrix.h"
#include "decompress/CrossSum/VerticalSumMatrix.h"
#include "decompress/CrossSum/DiagonalSumMatrix.h"
#include "decompress/CrossSum/AntiDiagonalSumMatrix.h"
#include "common/O11y/O11y.h"

namespace crsce::decompress {

bool Decompressor::solve_block(std::span<const std::uint8_t> lh,
                               const CrossSums &sums,
                               Csm &csm_out,
                               const std::uint64_t valid_bits) const {
    constexpr std::size_t S = Csm::kS;
    static constexpr int kMaxDeIters = 60000;

    const auto &lsm = sums.lsm().targets();
    const auto &vsm = sums.vsm().targets();
    const auto &dsm = sums.dsm().targets();
    const auto &xsm = sums.xsm().targets();

    ConstraintState st(S, lsm, vsm, dsm, xsm);
    BlockSolveSnapshot snap{S,
                            st,
                            std::span<const std::uint16_t>(lsm.begin(), lsm.end()),
                            std::span<const std::uint16_t>(vsm.begin(), vsm.end()),
                            std::span<const std::uint16_t>(dsm.begin(), dsm.end()),
                            std::span<const std::uint16_t>(xsm.begin(), xsm.end()),
                            0ULL};
    set_block_solve_snapshot(snap);

    ::crsce::decompress::detail::prelock_padded_tail(csm_out, st, valid_bits);
    // Construct a fresh primary solver per block using the central
    // SelectedSolver factory. This binds the solver to this block's Csm and
    // ConstraintState without exposing concrete types here. Construction is
    // deferred to ensure correct per-block state and to avoid any reuse bugs.
    if (const auto solver = ::crsce::decompress::solvers::selected::make_primary_solver(csm_out, st, solver_cfg_)) {
        solver->solve();
    }
    return ::crsce::decompress::detail::verify_cross_sums_and_lh(csm_out, sums, lh, snap);
}

} // namespace crsce::decompress
