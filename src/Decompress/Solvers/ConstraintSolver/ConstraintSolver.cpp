/**
 * @file ConstraintSolver.cpp
 * @brief Implementation of residual-driven ConstraintSolver::solve
 *        (DE → BitSplash → Radditz → Hybrid Accel fallback to Hybrid).
 * @author Sam Caldwell
 * © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#include "decompress/Solvers/ConstraintSolver/ConstraintSolver.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Block/detail/get_block_solve_snapshot.h"
#include "decompress/Block/detail/SnapshotGuard.h"
#include "decompress/Block/detail/seed_initial_beliefs.h"
#include "decompress/Block/detail/execute_bitsplash_and_validate.h"
#include "decompress/Block/detail/reseed_residuals_from_csm.h"
#include "decompress/Block/detail/run_hybrid_sift_accel.h"
#include "decompress/HashMatrix/LateralHashMatrix.h"
#include "decompress/CrossSum/LateralSumMatrix.h"
#include "decompress/CrossSum/VerticalSumMatrix.h"
#include "decompress/CrossSum/DiagonalSumMatrix.h"
#include "decompress/CrossSum/AntiDiagonalSumMatrix.h"
#include "decompress/Phases/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/CrossSum/CrossSums.h"
#include "decompress/Solvers/GenericSolver.h"
#include "common/O11y/O11y.h"

namespace crsce::decompress::solvers::constraint {

ConstraintSolver::ConstraintSolver(Csm &csm,
                                   ConstraintState &st,
                                   const CrossSums &sums,
                                   const std::span<const std::uint8_t> lh) noexcept
    : GenericSolver(csm, st), sums_(sums), lh_(lh) {}

void ConstraintSolver::solve(std::size_t max_iters) {
    (void)max_iters; // composite solver; runs once
    constexpr std::size_t S = Csm::kS;
    static constexpr int kMaxDeIters = 60000;

    // Acquire or initialize a live snapshot for telemetry
    auto snap_opt = ::crsce::decompress::get_block_solve_snapshot();
    BlockSolveSnapshot snap = snap_opt.has_value()
        ? *snap_opt
        : BlockSolveSnapshot(
              Csm::kS,
              state(),
              std::span<const std::uint16_t>(sums_.lsm().targets().begin(), sums_.lsm().targets().end()),
              std::span<const std::uint16_t>(sums_.vsm().targets().begin(), sums_.vsm().targets().end()),
              std::span<const std::uint16_t>(sums_.dsm().targets().begin(), sums_.dsm().targets().end()),
              std::span<const std::uint16_t>(sums_.xsm().targets().begin(), sums_.xsm().targets().end()),
              0ULL);
    const ::crsce::decompress::detail::SnapshotGuard pub(snap);
    snap.message = "constraint_enter";

    // Deterministic elimination
    ::crsce::o11y::O11y::instance().event("constraint_de_phase_start");
    const ::crsce::decompress::hashes::LateralHashMatrix lhm{lh_};
    DeterministicElimination det(kMaxDeIters, csm(), state(), snap, lhm);
    (void)det.run(); // Continue regardless; downstream phases and final verification gate correctness
    if (det.solved()) {
        return;
    }

    // Seed initial beliefs
    const std::uint64_t belief_seed = ::crsce::decompress::detail::seed_initial_beliefs(csm(), state());
    snap.rng_seed_belief = belief_seed;

    // Typed target matrices
    const auto &lsm = sums_.lsm().targets();
    const auto &vsm = sums_.vsm().targets();
    const auto &dsm = sums_.dsm().targets();
    const auto &xsm = sums_.xsm().targets();
    const ::crsce::decompress::xsum::LateralSumMatrix  LSM{
        std::span<const std::uint16_t>(lsm.begin(), lsm.end())
    };
    const ::crsce::decompress::xsum::VerticalSumMatrix VSM{
        std::span<const std::uint16_t>(vsm.begin(), vsm.end())
    };
    const ::crsce::decompress::xsum::DiagonalSumMatrix DSM{
        std::span<const std::uint16_t>(dsm.begin(), dsm.end())
    };
    const ::crsce::decompress::xsum::AntiDiagonalSumMatrix XSM{
        std::span<const std::uint16_t>(xsm.begin(), xsm.end())
    };

    // BitSplash rows completion
    (void) ::crsce::decompress::detail::execute_bitsplash_and_validate(csm(), state(), snap, LSM);

    // Refresh residuals using CSM counters
    ::crsce::decompress::detail::reseed_residuals_from_csm(csm(), state(), LSM, VSM, DSM, XSM);

    // Hybrid sift with acceleration wrapper (GPU TopK if present, else fallback)
    ::crsce::o11y::O11y::instance().event("constraint_hybrid_accel");
    const ::crsce::decompress::hashes::LateralHashMatrix LHM_fb{lh_};
    (void) ::crsce::decompress::detail::run_hybrid_sift_accel(csm(), state(), snap, LHM_fb, VSM, DSM, XSM);

    // No post-hybrid micro-solvers; final verification applies immediately per specification.
}

} // namespace crsce::decompress::solvers::constraint
