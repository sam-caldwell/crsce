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

    snap.phase = BlockSolveSnapshot::Phase::gobp;
    snap.gobp_status = 1;
    ::crsce::o11y::event("gobp_start_simple");
    set_block_solve_snapshot(snap);

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
    const auto sys_start_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    // Stall breaker configuration (env-tunable)
    std::uint64_t stall_ms_thresh = 15000ULL; // default 15s
    if (const char *p = std::getenv("CRSCE_GOBP_STALL_MS")) { // NOLINT(concurrency-mt-unsafe)
        if (const std::int64_t v_ll = std::strtoll(p, nullptr, 10); v_ll > 0) {
            stall_ms_thresh = static_cast<std::uint64_t>(v_ll);
        }
    }
    std::uint64_t phase0_advance_ms = stall_ms_thresh; // reuse unless overridden
    if (const char *p = std::getenv("CRSCE_GOBP_PHASE0_ADV_MS")) { // NOLINT(concurrency-mt-unsafe)
        if (const std::int64_t v_ll = std::strtoll(p, nullptr, 10); v_ll > 0) {
            phase0_advance_ms = static_cast<std::uint64_t>(v_ll);
        }
    }
    unsigned boost_passes = 8U;
    if (const char *p = std::getenv("CRSCE_GOBP_STALL_BOOST_PASSES")) { // NOLINT(concurrency-mt-unsafe)
        if (const std::int64_t v_ll = std::strtoll(p, nullptr, 10); v_ll > 0) {
            boost_passes = static_cast<unsigned>(v_ll);
        }
    }
    unsigned boost_samples = 256U;
    if (const char *p = std::getenv("CRSCE_GOBP_STALL_BOOST_SAMPLES")) { // NOLINT(concurrency-mt-unsafe)
        if (const std::int64_t v_ll = std::strtoll(p, nullptr, 10); v_ll > 0) {
            boost_samples = static_cast<unsigned>(v_ll);
        }
    }
    unsigned boost_accepts = 16U;
    if (const char *p = std::getenv("CRSCE_GOBP_STALL_BOOST_ACCEPTS")) { // NOLINT(concurrency-mt-unsafe)
        if (const std::int64_t v_ll = std::strtoll(p, nullptr, 10); v_ll > 0) {
            boost_accepts = static_cast<unsigned>(v_ll);
        }
    }
    bool early_exit_on_stall = true;
    if (const char *p = std::getenv("CRSCE_GOBP_STALL_EARLY_EXIT")) { // NOLINT(concurrency-mt-unsafe)
        if (*p == '0') { early_exit_on_stall = false; }
    }

    bool stall_active = false;
    unsigned stall_boost_remaining = 0U;
    std::size_t stall_baseline_accepts = 0U;

    for (std::size_t ph = 0; ph < dampers.size() && !det.solved(); ++ph) {
        // Post‑Radditz: forbid single‑cell assignments; only allow preserve‑row/col swaps.
        constexpr std::array<double, 4> confs{{1.0, 1.0, 1.0, 1.0}};
        constexpr std::array<int, 4> iters{{6000, 9000, 80000, 120000}};
        gobp.set_damping(dampers.at(ph));
        gobp.set_assign_confidence(confs.at(ph));
        snap.gobp_phase_index = ph;
        // Initialize subphase scheduler (1=dx_focus, 2=lh_focus)
        std::int64_t dx_focus_iters = 2048;
        std::int64_t lh_focus_iters = 4096;
        if (const char *p1 = std::getenv("CRSCE_DX_FOCUS_ITERS")) { // NOLINT(concurrency-mt-unsafe)
            const std::int64_t v = std::strtoll(p1, nullptr, 10);
            if (v > 0) { dx_focus_iters = v; }
        }
        if (const char *p2 = std::getenv("CRSCE_LH_FOCUS_ITERS")) { // NOLINT(concurrency-mt-unsafe)
            const std::int64_t v = std::strtoll(p2, nullptr, 10);
            if (v > 0) { lh_focus_iters = v; }
        }
        int subphase = 1; // start with DX focus
        std::int64_t sub_iters = dx_focus_iters;
        snap.gobp_subphase = subphase;
        set_block_solve_snapshot(snap);
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
            if (prog > 0) { snap.gobp_cells_solved_total += prog; }
            ++snap.gobp_iters_run;
            if (prog > 0) {
                snap.gobp_last_accept_iter = snap.gobp_iters_run;
            }
            // Do not run DE here: after Radditz, value changes must preserve row/col; DE assigns cells.
            // If no direct assignments happened, attempt a few preserve-row/col rectangle swaps
            bool publish = (prog > 0);
            if (prog == 0) {
                // Stall detection (time since last accept)
                const auto now_ms = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
                const std::uint64_t last_ms = (snap.gobp_last_accept_ms != 0U) ? snap.gobp_last_accept_ms : sys_start_ms;
                const std::uint64_t stall_ms = (now_ms > last_ms) ? (now_ms - last_ms) : 0ULL;
                if (!stall_active && stall_ms >= stall_ms_thresh) {
                    stall_active = true;
                    stall_boost_remaining = boost_passes;
                    stall_baseline_accepts = snap.gobp_rect_accepts_total;
                    ::crsce::o11y::event("gobp_stall_boost_start");
                }

                const unsigned samples = stall_active ? boost_samples : 48U;
                const unsigned accepts = stall_active ? boost_accepts : 4U;
                const std::size_t gained = ::crsce::decompress::detail::gobp_preserve_rowcol_swap(
                    csm, st, lh, dsm, xsm, samples, accepts);
                ++snap.gobp_rect_passes;
                snap.gobp_rect_attempts_total += samples;
                snap.gobp_rect_accepts_total += gained;
                if (gained > 0) {
                    const auto now_ms = static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count());
                    snap.gobp_last_accept_ms = now_ms;
                    snap.gobp_last_accept_iter = snap.gobp_iters_run;
                    publish = true;
                    stall_active = false; // leave boost mode on any accept
                } else if (stall_active) {
                    if (stall_boost_remaining > 0U) {
                        --stall_boost_remaining;
                    }
                    if (stall_boost_remaining == 0U && early_exit_on_stall &&
                        snap.gobp_rect_accepts_total == stall_baseline_accepts) {
                        ::crsce::o11y::event("gobp_stall_early_exit");
                        snap.phase = BlockSolveSnapshot::Phase::endOfIterations;
                        snap.gobp_status = 2; // failed
                        set_block_solve_snapshot(snap);
                        return false;
                    }
                }
            }
            // No valid-prefix commits: after Radditz, preserve row/col counts strictly.
            std::size_t sumU = 0; for (const auto u : st.U_row) {
                sumU += static_cast<std::size_t>(u);
            }
            snap.unknown_total = sumU;
            snap.solved = (S * S) - sumU;
            // Iter distance to last accept
            if (snap.gobp_iters_run >= snap.gobp_last_accept_iter) {
                snap.gobp_iters_since_accept = snap.gobp_iters_run - snap.gobp_last_accept_iter;
            } else {
                snap.gobp_iters_since_accept = 0;
            }
            // Update short-window accept rate (basis points) every 256 rectangle passes (delta over last window)
            if ((snap.gobp_rect_passes - snap.gobp_rate_ckpt_passes) >= 256U) {
                const std::size_t d_acc = snap.gobp_rect_accepts_total - snap.gobp_rate_ckpt_accepts;
                const std::size_t d_att = snap.gobp_rect_attempts_total - snap.gobp_rate_ckpt_attempts;
                const std::size_t att_nz = (d_att == 0U) ? 1U : d_att;
                const std::size_t rate_bp = (d_acc * 10000U) / att_nz; // 10000 bp = 100%
                snap.gobp_accept_rate_bp = static_cast<std::uint16_t>((rate_bp > 10000U) ? 10000U : rate_bp);
                snap.gobp_rate_ckpt_passes = snap.gobp_rect_passes;
                snap.gobp_rate_ckpt_accepts = snap.gobp_rect_accepts_total;
                snap.gobp_rate_ckpt_attempts = snap.gobp_rect_attempts_total;
            }
            // Subphase toggle window
            if (--sub_iters <= 0) {
                if (subphase == 1) { subphase = 2; sub_iters = lh_focus_iters; }
                else { subphase = 1; sub_iters = dx_focus_iters; }
                snap.gobp_subphase = subphase;
                set_block_solve_snapshot(snap);
            }
            // Early phase advance: if stalling in phase 0, skip ahead
            if (ph == 0) {
                const auto now_ms2 = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
                const std::uint64_t last_ms2 = (snap.gobp_last_accept_ms != 0U) ? snap.gobp_last_accept_ms : sys_start_ms;
                const std::uint64_t stall_ms2 = (now_ms2 > last_ms2) ? (now_ms2 - last_ms2) : 0ULL;
                if (stall_ms2 >= phase0_advance_ms) {
                    ::crsce::o11y::event("gobp_phase0_advance");
                    break; // advance to next phase
                }
            }
            // Publish incremental progress for heartbeat visibility (throttled)
            if (publish || ((snap.gobp_iters_run & 63U) == 0U)) {
                set_block_solve_snapshot(snap);
            }
        }
    }
    if (!det.solved()) {
        ::crsce::o11y::event("gobp_failed_to_solve");
        snap.phase = BlockSolveSnapshot::Phase::endOfIterations;
        set_block_solve_snapshot(snap);
        return false;
    }
    snap.gobp_status = 3;
    set_block_solve_snapshot(snap);
    return true;
}

} // namespace crsce::decompress::detail
