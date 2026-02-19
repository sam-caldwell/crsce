/**
 * @file PipelineSolver.cpp
 * @brief Implementation of PipelineSolver::solve (DE→BitSplash→Radditz→Hybrid Sift).
 */

#include "decompress/Solvers/PipelineSolver/PipelineSolver.h"

#include <cstddef>

#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Block/detail/get_block_solve_snapshot.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "decompress/Block/detail/seed_initial_beliefs.h"
#include "decompress/Block/detail/execute_bitsplash_and_validate.h"
#include "decompress/Block/detail/execute_radditz_and_validate.h"
#include "decompress/Block/detail/reseed_residuals_from_csm.h"
#include "decompress/Block/detail/run_hybrid_sift.h"
#include "decompress/HashMatrix/LateralHashMatrix.h"
#include "decompress/CrossSum/LateralSumMatrix.h"
#include "decompress/CrossSum/VerticalSumMatrix.h"
#include "decompress/CrossSum/DiagonalSumMatrix.h"
#include "decompress/CrossSum/AntiDiagonalSumMatrix.h"
#include "decompress/Phases/DeterministicElimination/DeterministicElimination.h"
#include "common/O11y/O11y.h"

namespace crsce::decompress::solvers::pipeline {

void PipelineSolver::solve(std::size_t) {
    constexpr std::size_t S = Csm::kS;
    static constexpr int kMaxDeIters = 60000;

    // Acquire the live snapshot (constructed by Decompressor::solve_block) for telemetry updates
    const auto snap_opt = ::crsce::decompress::get_block_solve_snapshot();
    if (!snap_opt.has_value()) {
        return; // cannot publish telemetry; upstream verify will still run
    }
    auto snap = *snap_opt;

    // Deterministic elimination first
    ::crsce::o11y::O11y::instance().event("block_start_de_phase");
    const ::crsce::decompress::hashes::LateralHashMatrix lhm{lh_};
    DeterministicElimination det(kMaxDeIters, csm(), state(), snap, lhm);
    if (!det.run()) {
        set_block_solve_snapshot(snap);
        return; // upstream verify will fail
    }
    if (det.solved()) {
        set_block_solve_snapshot(snap);
        return; // solved by DE; verification occurs upstream
    }

    // Seed initial beliefs from current CSM topology
    const std::uint64_t belief_seed = ::crsce::decompress::detail::seed_initial_beliefs(csm(), state());
    snap.rng_seed_belief = belief_seed;

    // Construct typed target matrices from original cross-sum vectors
    const auto &lsm = sums_.lsm().targets();
    const auto &vsm = sums_.vsm().targets();
    const auto &dsm = sums_.dsm().targets();
    const auto &xsm = sums_.xsm().targets();
    const ::crsce::decompress::xsum::LateralSumMatrix  LSM{std::span<const std::uint16_t>(lsm.begin(), lsm.end())};
    const ::crsce::decompress::xsum::VerticalSumMatrix VSM{std::span<const std::uint16_t>(vsm.begin(), vsm.end())};
    const ::crsce::decompress::xsum::DiagonalSumMatrix DSM{std::span<const std::uint16_t>(dsm.begin(), dsm.end())};
    const ::crsce::decompress::xsum::AntiDiagonalSumMatrix XSM{std::span<const std::uint16_t>(xsm.begin(), xsm.end())};

    // BitSplash to complete rows; bail if mismatch
    if (!::crsce::decompress::detail::execute_bitsplash_and_validate(csm(), state(), snap, LSM)) {
        set_block_solve_snapshot(snap);
        return;
    }

    // Column-focused Radditz; validate columns + row invariant
    if (!::crsce::decompress::detail::execute_radditz_and_validate(csm(), state(), snap, LSM, VSM)) {
        set_block_solve_snapshot(snap);
        return;
    }

    // Refresh residuals using canonical counters from CSM
    ::crsce::decompress::detail::reseed_residuals_from_csm(csm(), state(), LSM, VSM, DSM, XSM);

    // Hybrid sift guided by beliefs over DSM/XSM; no hard validation here
    ::crsce::o11y::O11y::instance().event("hybrid_sift");
    const ::crsce::decompress::hashes::LateralHashMatrix LHM_fb{lh_};
    (void) ::crsce::decompress::detail::run_hybrid_sift(csm(), state(), snap, LHM_fb, DSM, XSM);

    // Publish final snapshot state for this block; upstream performs final verification
    set_block_solve_snapshot(snap);
}

} // namespace crsce::decompress::solvers::pipeline
