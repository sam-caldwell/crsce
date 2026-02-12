/**
 * @file BlockSolver_gobp.cpp
 * @brief Implementation of run_gobp_fallback extracted from solve_block.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */

#include "decompress/Block/detail/run_gobp_fallback.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint> // NOLINT

#include "decompress/Block/detail/pre_polish_commit_valid_prefix.h"
#include <span>
#include "decompress/Phases/Gobp/GobpSolver.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "common/O11y/event.h"

namespace crsce::decompress::detail {

/**
 * @name run_gobp_fallback
 * @brief Lightweight GOBP refinement loop run after BitSplash/Radditz.
 *        Uses a small, fixed multi‑phase schedule and periodically runs short
 *        DE propagations plus valid‑prefix commits. Updates timing and status in `snap`.
 * @param csm Cross‑Sum Matrix to update (in place).
 * @param st Constraint state (R/U) to update (in place).
 * @param det DeterministicElimination engine for periodic propagations.
 * @param baseline_csm Baseline CSM for prefix‑commit comparisons.
 * @param baseline_st Baseline constraints for prefix‑commit comparisons.
 * @param lh LH digest bytes for prefix verification.
 * @param lsm Row targets (unused by this simplified fallback; kept for signature stability).
 * @param vsm Column targets (unused by this simplified fallback).
 * @param dsm Diagonal targets (unused by this simplified fallback).
 * @param xsm Anti‑diagonal targets (unused by this simplified fallback).
 * @param snap Snapshot to record timings, counters, and status codes.
 * @param valid_bits Number of meaningful bits in the last block (unused here).
 * @return bool True if `det.solved()` when the schedule completes; false otherwise.
 */
bool run_gobp_fallback(Csm &csm,
                       ConstraintState &st,
                       DeterministicElimination &det,
                       Csm &baseline_csm,
                       ConstraintState &baseline_st,
                       const std::span<const std::uint8_t> lh,
                       const std::span<const std::uint16_t> lsm,
                       const std::span<const std::uint16_t> vsm,
                       const std::span<const std::uint16_t> dsm,
                       const std::span<const std::uint16_t> xsm,
                       BlockSolveSnapshot &snap,
                       const std::uint64_t valid_bits) {

    (void)lsm;
    (void)vsm;
    (void)dsm;
    (void)xsm;
    (void)valid_bits;

    constexpr std::size_t S = Csm::kS;
    if (det.solved()) {
        return true;
    }

    snap.gobp_status = 1;
    ::crsce::o11y::event("gobp_start_simple");

    constexpr std::array<double, 4> dampers{{0.50, 0.12, 0.35, 0.03}};

    ::crsce::decompress::GobpSolver gobp{csm, st};
    gobp.set_scan_flipped(false);

    for (std::size_t ph = 0; ph < dampers.size() && !det.solved(); ++ph) {
        constexpr std::array<double, 4> confs{{0.995, 0.72, 0.86, 0.58}};
        constexpr std::array<int, 4> iters{{6000, 9000, 80000, 120000}};
        gobp.set_damping(dampers.at(ph));
        gobp.set_assign_confidence(confs.at(ph));
        for (int i = 0; i < iters.at(ph) && !det.solved(); ++i) {
            const auto t0g = std::chrono::steady_clock::now();
            (void)gobp.solve_step();
            const auto t1g = std::chrono::steady_clock::now();
            snap.time_gobp_ms += static_cast<std::size_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(t1g - t0g).count());
            for (int k = 0; k < 8; ++k) {
                if (det.solve_step() == 0) {
                    break;
                }
            }
            if ((i % 2000) == 0) {
                (void)::crsce::decompress::detail::commit_valid_prefix(
                    csm,
                    st,
                    lh,
                    baseline_csm,
                    baseline_st,
                    snap,
                    /*rs*/0
                );
            }
            std::size_t sumU = 0; for (const auto u : st.U_row) {
                sumU += static_cast<std::size_t>(u);
            }
            snap.unknown_total = sumU;
            snap.solved = (S * S) - sumU;
        }
    }
    if (!det.solved()) {
        ::crsce::o11y::event("gobp_failed_to_solve");
        snap.phase = BlockSolveSnapshot::Phase::endOfIterations;
        set_block_solve_snapshot(snap);
        return false;
    }
    snap.gobp_status = 3;
    return true;
}

} // namespace crsce::decompress::detail
