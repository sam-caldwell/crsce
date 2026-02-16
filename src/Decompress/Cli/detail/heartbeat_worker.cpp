/**
 * @file heartbeat_worker.cpp
 * @brief Implementation of decompressor CLI heartbeat thread entrypoint.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#include "decompress/Cli/detail/heartbeat_worker.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <fstream>
#include <ios>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

#include "decompress/Block/detail/get_block_solve_snapshot.h"
#include "decompress/Decompressor/detail/phase_to_cstr.h"
#include "common/O11y/O11y.h"
#include "decompress/Cli/detail/heartbeat_pct_sat.h"
#include "decompress/Cli/detail/heartbeat_sum_unknowns.h"

namespace crsce::decompress::cli::detail {
    /**
     * @name heartbeat_worker
     * @brief Background worker that periodically prints a heartbeat line to stdout
     *        containing the current timestamp, phase, and progress metrics.
     * @param run_flag Pointer to an atomic boolean. When false, the worker exits.
     * @param interval_ms Interval between heartbeats in milliseconds.
     * @return void
     */
    void heartbeat_worker(std::atomic<bool> *run_flag, const unsigned interval_ms) {
        using namespace std::chrono;
        const char *hb_path = std::getenv("CRSCE_HEARTBEAT_PATH"); // NOLINT(concurrency-mt-unsafe)
        std::ofstream hb_stream;
        if (hb_path && *hb_path) {
            hb_stream.open(hb_path, std::ios::out | std::ios::app);
        }
        // Record process id once for inclusion in each heartbeat line
        std::uint64_t pid_ul = 0ULL;
#if defined(__unix__) || defined(__APPLE__)
        pid_ul = static_cast<std::uint64_t>(::getpid());
#endif
        while (run_flag && run_flag->load(std::memory_order_relaxed)) {
            const auto now = system_clock::now();
            const auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count();
            const auto ts_ms = static_cast<std::int64_t>(ms);
            const auto snap_opt = ::crsce::decompress::get_block_solve_snapshot();
            std::ostringstream oss;
            if (snap_opt) {
                const auto &s = *snap_opt;
                const char *ph = ::crsce::decompress::detail::phase_to_cstr(s.phase);
                std::string ph_label = ph ? std::string(ph) : std::string("init");
                if (s.phase == ::crsce::decompress::BlockSolveSnapshot::Phase::radditzSift) {
                    if (s.radditz_kind == 2) {
                        ph_label = "radditz-sift-dsm";
                    } else if (s.radditz_kind == 3) {
                        ph_label = "radditz-sift-xsm";
                    } else {
                        ph_label = "radditz-sift-vsm";
                    }
                }
                oss << '[' << ts_ms << "] pid=" << pid_ul << " phase=" << ph_label
                    << " solved=" << static_cast<std::uint64_t>(s.solved)
                    << " unknown=" << static_cast<std::uint64_t>(s.unknown_total);
                // Diagonal/anti-diagonal satisfaction (percentage) when U vectors are available
                const int lsm_sat_pct = pct_sat(s.U_row);
                const int vsm_sat_pct = pct_sat(s.U_col);
                const int dsm_sat_pct = pct_sat(s.U_diag);
                const int xsm_sat_pct = pct_sat(s.U_xdiag);
                // Emit cross-sum satisfaction and unknown totals as o11y metrics
                ::crsce::o11y::O11y::instance().metric("crosssum.sat_pct", static_cast<std::int64_t>(lsm_sat_pct), {{"family","lsm"}});
                ::crsce::o11y::O11y::instance().metric("crosssum.sat_pct", static_cast<std::int64_t>(vsm_sat_pct), {{"family","vsm"}});
                ::crsce::o11y::O11y::instance().metric("crosssum.sat_pct", static_cast<std::int64_t>(dsm_sat_pct), {{"family","dsm"}});
                ::crsce::o11y::O11y::instance().metric("crosssum.sat_pct", static_cast<std::int64_t>(xsm_sat_pct), {{"family","xsm"}});
                ::crsce::o11y::O11y::instance().metric("crosssum.unknown_total", sum_unknowns(s.U_row), {{"family","lsm"}});
                ::crsce::o11y::O11y::instance().metric("crosssum.unknown_total", sum_unknowns(s.U_col), {{"family","vsm"}});
                ::crsce::o11y::O11y::instance().metric("crosssum.unknown_total", sum_unknowns(s.U_diag), {{"family","dsm"}});
                ::crsce::o11y::O11y::instance().metric("crosssum.unknown_total", sum_unknowns(s.U_xdiag), {{"family","xsm"}});
                switch (s.phase) {
                    case ::crsce::decompress::BlockSolveSnapshot::Phase::de:
                        oss << ",de_status=" << s.de_status;
                        break;
                    case ::crsce::decompress::BlockSolveSnapshot::Phase::rowPhase:
                        oss << ",rows_it=" << static_cast<std::uint64_t>(s.row_phase_iterations)
                            << ",row_status=" << s.bitsplash_status;
                        break;
                    case ::crsce::decompress::BlockSolveSnapshot::Phase::radditzSift: {
                        // For VSM/XSM keep original counters; for DSM show custom live metrics
                        if (s.radditz_kind == 2) {
                            // DSM-focused: show passes, VSM pass flag, and speeds
                            oss << ",r_passes=" << static_cast<std::uint64_t>(s.radditz_passes_last);
                            const int c_passes = (s.radditz_cols_remaining == 0 ? 1 : 0);
                            oss << ",c_passes=" << c_passes;
                            // Compute per-second rates based on last heartbeat deltas
                            static std::uint64_t prev_ts_ms = 0ULL;
                            static std::uint64_t prev_rect = 0ULL;
                            static std::uint64_t prev_swaps = 0ULL;
                            static bool have_prev = false;
                            std::uint64_t rects_per_sec = 0ULL;
                            std::uint64_t swaps_per_sec = 0ULL;
                            const auto ts_now = static_cast<std::uint64_t>(ts_ms);
                            if (have_prev && ts_now > prev_ts_ms) {
                                const std::uint64_t dt_ms = ts_now - prev_ts_ms;
                                const std::uint64_t drect = (s.dsm_rectangles_chosen >= prev_rect) ? (s.dsm_rectangles_chosen - prev_rect) : 0ULL;
                                const std::uint64_t dswap = (s.dsm_swaps >= prev_swaps) ? (s.dsm_swaps - prev_swaps) : 0ULL;
                                rects_per_sec = (dt_ms > 0) ? ((drect * 1000ULL) / dt_ms) : 0ULL;
                                swaps_per_sec = (dt_ms > 0) ? ((dswap * 1000ULL) / dt_ms) : 0ULL;
                            }
                            prev_ts_ms = ts_now;
                            prev_rect = s.dsm_rectangles_chosen;
                            prev_swaps = s.dsm_swaps;
                            have_prev = true;
                            oss << ",rectangles_per_sec=" << rects_per_sec
                                << ",num_swaps_per_sec=" << swaps_per_sec
                                << ",num_rectangles_chosen=" << static_cast<std::uint64_t>(s.dsm_rectangles_chosen)
                                << ",num_swaps=" << static_cast<std::uint64_t>(s.dsm_swaps)
                                << ",dsm_err_min=" << static_cast<unsigned int>(s.dsm_err_min)
                                << ",dsm_err_max=" << static_cast<unsigned int>(s.dsm_err_max)
                                << ",dsm_avg_pct=" << s.dsm_avg_pct;
                        } else {
                            // VSM/XSM (original)
                            oss << ",r_cols_left=" << static_cast<std::uint64_t>(s.radditz_cols_remaining)
                                << ",r_passes=" << static_cast<std::uint64_t>(s.radditz_passes_last);
                        }
                        break;
                    }
                    case ::crsce::decompress::BlockSolveSnapshot::Phase::gobp: {
                        // Rich GOBP internals to distinguish live progress vs. stalls
                        oss << ",gobp_status=" << s.gobp_status
                            << ",phase_idx=" << static_cast<std::uint64_t>(s.gobp_phase_index)
                            << ",iters=" << static_cast<std::uint64_t>(s.gobp_iters_run)
                            << ",iters_since_accept=" << static_cast<std::uint64_t>(s.gobp_iters_since_accept)
                            << ",rect_passes=" << static_cast<std::uint64_t>(s.gobp_rect_passes)
                            << ",rect_acc/att=" << static_cast<std::uint64_t>(s.gobp_rect_accepts_total)
                            << '/' << static_cast<std::uint64_t>(s.gobp_rect_attempts_total)
                            << ",acc_rate_bp=" << static_cast<unsigned int>(s.gobp_accept_rate_bp)
                            << ",cells=" << static_cast<std::uint64_t>(s.gobp_cells_solved_total)
                            << ",rows_committed=" << static_cast<std::uint64_t>(s.rows_committed)
                            << ",cols_finished=" << static_cast<std::uint64_t>(s.cols_finished)
                            << ",subphase=" << (s.gobp_subphase == 2 ? "lh" : "dx")
                            << ",sat_lost=" << static_cast<std::uint64_t>(s.gobp_sat_lost)
                            << ",sat_gained=" << static_cast<std::uint64_t>(s.gobp_sat_gained);
                        if (dsm_sat_pct >= 0) { oss << ",dsm_sat_pct=" << dsm_sat_pct; }
                        if (xsm_sat_pct >= 0) { oss << ",xsm_sat_pct=" << xsm_sat_pct; }
                        // Derive stall_ms since last accepted rectangle (0 if none yet)
                        if (s.gobp_last_accept_ms != 0U) {
                            const std::uint64_t now_u = (ts_ms >= 0)
                                                         ? static_cast<std::uint64_t>(ts_ms)
                                                         : 0ULL;
                            const std::uint64_t stall_ms = (now_u > s.gobp_last_accept_ms)
                                                           ? (now_u - s.gobp_last_accept_ms)
                                                           : 0ULL;
                            oss << ",stall_ms=" << stall_ms;
                        } else {
                            oss << ",stall_ms=na";
                        }
                        break;
                    }
                    default:
                        break;
                }
            } else {
                oss << '[' << ts_ms << "] pid=" << pid_ul << " phase=init";
            }
            oss << '\n';
            const std::string line = oss.str();
            std::fputs(line.c_str(), stdout);
            std::fflush(stdout);
            if (hb_stream.is_open()) {
                hb_stream << line;
                hb_stream.flush();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }
        if (hb_stream.is_open()) { hb_stream.close(); }
    }
}
