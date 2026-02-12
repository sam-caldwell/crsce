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
#include <cstdlib>

#include <span>
#include "decompress/Phases/Gobp/GobpSolver.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "decompress/Block/detail/gobp_preserve_rowcol_swap.h"
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
                       [[ maybe_unused]] Csm & baseline_csm,
                       [[ maybe_unused]] ConstraintState & baseline_st,
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

    // Optional wall-clock time budget for the fallback, in milliseconds.
    // If set (CRSCE_GOBP_TIMEOUT_MS>0), we will abort early and return false
    // so callers can emit failure telemetry and completion_stats.log.
    std::uint64_t timeout_ms = 0;
    if (const char *p = std::getenv("CRSCE_GOBP_TIMEOUT_MS")) { // NOLINT(concurrency-mt-unsafe)
        if (const std::int64_t v_ll = std::strtoll(p, nullptr, 10); v_ll > 0) {
            timeout_ms = static_cast<std::uint64_t>(v_ll);
        }
    }
    const auto t_start = std::chrono::steady_clock::now();

    for (std::size_t ph = 0; ph < dampers.size() && !det.solved(); ++ph) {
        // Post‑Radditz: forbid single‑cell assignments; only allow preserve‑row/col swaps.
        constexpr std::array<double, 4> confs{{1.0, 1.0, 1.0, 1.0}};
        constexpr std::array<int, 4> iters{{6000, 9000, 80000, 120000}};
        gobp.set_damping(dampers.at(ph));
        gobp.set_assign_confidence(confs.at(ph));
        for (int i = 0; i < iters.at(ph) && !det.solved(); ++i) {
            // Check the optional wall-clock budget
            if (timeout_ms > 0) {
                const auto now = std::chrono::steady_clock::now();
                const std::uint64_t elapsed_ms = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - t_start).count());
                if (elapsed_ms >= timeout_ms) {
                    ::crsce::o11y::event("gobp_timeout");
                    snap.phase = BlockSolveSnapshot::Phase::endOfIterations;
                    set_block_solve_snapshot(snap);
                    return false;
                }
            }
            const auto t0g = std::chrono::steady_clock::now();
            const std::size_t prog = gobp.solve_step();
            const auto t1g = std::chrono::steady_clock::now();
            snap.time_gobp_ms += static_cast<std::size_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(t1g - t0g).count());
            // Do not run DE here: after Radditz, value changes must preserve row/col; DE assigns cells.
            // If no direct assignments happened, attempt a few preserve-row/col rectangle swaps
            if (prog == 0) {
                constexpr unsigned samples = 48; // bounded work; tune via env if desired later
                constexpr unsigned accepts = 4;
                const std::size_t gained = ::crsce::decompress::detail::gobp_preserve_rowcol_swap(
                    csm, st, lh, dsm, xsm, samples, accepts);
                (void)gained;
            }
            // No valid-prefix commits: after Radditz, preserve row/col counts strictly.
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
