/**
 * @file Decompressor_solve_block.cpp
 * @brief Decompressor::solve_block implementation: orchestrates solver selection and phases per block.
 */

#include "decompress/Decompressor/Decompressor.h"

#include <cstddef>
#include <cstdint>
#include <span>

#include "decompress/Csm/Csm.h"
#include "decompress/CrossSum/CrossSum.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "decompress/Block/detail/prelock_padded_tail.h"
#include "decompress/Block/detail/verify_cross_sums_and_lh.h"
#include "decompress/Phases/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/run_hybrid_sift.h"
#include "decompress/Block/detail/seed_initial_beliefs.h"
#include "decompress/Block/detail/execute_bitsplash_and_validate.h"
#include "decompress/Block/detail/execute_radditz_and_validate.h"
#include "decompress/Block/detail/reseed_residuals_from_csm.h"
#include "decompress/HashMatrix/LateralHashMatrix.h"
#include "decompress/CrossSum/LateralSumMatrix.h"
#include "decompress/CrossSum/VerticalSumMatrix.h"
#include "decompress/CrossSum/DiagonalSumMatrix.h"
#include "decompress/CrossSum/AntiDiagonalSumMatrix.h"
#include "decompress/Solvers/SelectedSolver.h"
#include "common/O11y/O11y.h"

namespace crsce::decompress {

bool Decompressor::solve_block(const std::span<const std::uint8_t> lh,
                               const CrossSums &sums,
                               Csm &csm_out,
                               const std::uint64_t valid_bits) {
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

#if defined(CRSCE_SOLVER_PRIMARY_GOBP) || defined(CRSCE_USE_GOBP_ONLY)
    ::crsce::o11y::O11y::instance().event("gobp_driver_begin");
    ::crsce::decompress::detail::prelock_padded_tail(csm_out, st, valid_bits);
    // Construct selected primary solver and run until stall/solved
    {
        auto solver = ::crsce::decompress::solvers::selected::make_primary_solver(csm_out, st, solver_cfg_);
        solver->solve();
    }
    if (::crsce::decompress::detail::verify_cross_sums_and_lh(csm_out, sums, lh, snap)) {
        return true;
    }
#ifdef CRSCE_ENABLE_SOLVER_FALLBACK
    // Fallback pipeline (DE → BitSplash → Radditz → Hybrid Sift)
    {
        ::crsce::o11y::O11y::instance().event("block_start_de_phase_fallback");
        const ::crsce::decompress::hashes::LateralHashMatrix lhm{lh};
        DeterministicElimination det(kMaxDeIters, csm_out, st, snap, lhm);
        if (!det.run()) { return false; }
        if (det.solved()) {
            return ::crsce::decompress::detail::verify_cross_sums_and_lh(csm_out, sums, lh, snap);
        }
    }
    const std::uint64_t belief_seed_fb = ::crsce::decompress::detail::seed_initial_beliefs(csm_out, st);
    snap.rng_seed_belief = belief_seed_fb;
    const ::crsce::decompress::xsum::LateralSumMatrix LSM_fb{std::span<const std::uint16_t>(lsm.begin(), lsm.end())};
    const ::crsce::decompress::xsum::VerticalSumMatrix VSM_fb{std::span<const std::uint16_t>(vsm.begin(), vsm.end())};
    const ::crsce::decompress::xsum::DiagonalSumMatrix DSM_fb{std::span<const std::uint16_t>(dsm.begin(), dsm.end())};
    const ::crsce::decompress::xsum::AntiDiagonalSumMatrix XSM_fb{std::span<const std::uint16_t>(xsm.begin(), xsm.end())};
    if (!::crsce::decompress::detail::execute_bitsplash_and_validate(csm_out, st, snap, LSM_fb)) { return false; }
    if (!::crsce::decompress::detail::execute_radditz_and_validate(csm_out, st, snap, LSM_fb, VSM_fb)) { return false; }
    ::crsce::decompress::detail::reseed_residuals_from_csm(csm_out, st, LSM_fb, VSM_fb, DSM_fb, XSM_fb);
    ::crsce::o11y::O11y::instance().event("hybrid_sift_fallback");
    const ::crsce::decompress::hashes::LateralHashMatrix LHM_fb{lh};
    (void) ::crsce::decompress::detail::run_hybrid_sift(csm_out, st, snap, LHM_fb, DSM_fb, XSM_fb);
    set_block_solve_snapshot(snap);
    return ::crsce::decompress::detail::verify_cross_sums_and_lh(csm_out, sums, lh, snap);
#else
    return false;
#endif
#else
    // Original pipeline without GOBP-only defines
    ::crsce::o11y::O11y::instance().event("block_start_de_phase");
    const ::crsce::decompress::hashes::LateralHashMatrix lhm{lh};
    DeterministicElimination det(kMaxDeIters, csm_out, st, snap, lhm);
    if (!det.run()) { return false; }
    if (det.solved()) {
        return ::crsce::decompress::detail::verify_cross_sums_and_lh(csm_out, sums, lh, snap);
    }
    ::crsce::decompress::detail::prelock_padded_tail(csm_out, st, valid_bits);
    const std::uint64_t belief_seed = ::crsce::decompress::detail::seed_initial_beliefs(csm_out, st);
    snap.rng_seed_belief = belief_seed;
    const ::crsce::decompress::xsum::LateralSumMatrix LSM{std::span<const std::uint16_t>(lsm.begin(), lsm.end())};
    const ::crsce::decompress::xsum::VerticalSumMatrix VSM{std::span<const std::uint16_t>(vsm.begin(), vsm.end())};
    const ::crsce::decompress::xsum::DiagonalSumMatrix DSM{std::span<const std::uint16_t>(dsm.begin(), dsm.end())};
    const ::crsce::decompress::xsum::AntiDiagonalSumMatrix XSM{std::span<const std::uint16_t>(xsm.begin(), xsm.end())};
    if (!::crsce::decompress::detail::execute_bitsplash_and_validate(csm_out, st, snap, LSM)) { return false; }
    if (!::crsce::decompress::detail::execute_radditz_and_validate(csm_out, st, snap, LSM, VSM)) { return false; }
    ::crsce::decompress::detail::reseed_residuals_from_csm(csm_out, st, LSM, VSM, DSM, XSM);
    ::crsce::o11y::O11y::instance().event("hybrid_sift");
    const ::crsce::decompress::hashes::LateralHashMatrix LHM{lh};
    (void) ::crsce::decompress::detail::run_hybrid_sift(csm_out, st, snap, LHM, DSM, XSM);
    set_block_solve_snapshot(snap);
    return ::crsce::decompress::detail::verify_cross_sums_and_lh(csm_out, sums, lh, snap);
#endif
}

} // namespace crsce::decompress
