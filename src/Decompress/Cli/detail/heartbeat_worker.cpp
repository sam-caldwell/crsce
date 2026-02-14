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

#include "decompress/Block/detail/get_block_solve_snapshot.h"
#include "decompress/Decompressor/detail/phase_to_cstr.h"

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
                oss << '[' << ts_ms << "] phase=" << ph_label
                    << " solved=" << static_cast<std::uint64_t>(s.solved)
                    << " unknown=" << static_cast<std::uint64_t>(s.unknown_total);
                // Diagonal/anti-diagonal satisfaction (percentage) when U vectors are available
                auto pct_sat = [](const std::vector<std::uint16_t> &u) -> int {
                    if (u.empty()) { return -1; }
                    std::size_t ok = 0;
                    for (auto v : u) { if (v == 0U) { ++ok; } }
                    const std::size_t n = u.size();
                    return static_cast<int>((ok * 100ULL) / (n ? n : 1ULL));
                };
                const int dsm_sat_pct = pct_sat(s.U_diag);
                const int xsm_sat_pct = pct_sat(s.U_xdiag);
                switch (s.phase) {
                    case ::crsce::decompress::BlockSolveSnapshot::Phase::de:
                        oss << ",de_status=" << s.de_status;
                        break;
                    case ::crsce::decompress::BlockSolveSnapshot::Phase::rowPhase:
                        oss << ",rows_it=" << static_cast<std::uint64_t>(s.row_phase_iterations)
                            << ",row_status=" << s.bitsplash_status;
                        break;
                    case ::crsce::decompress::BlockSolveSnapshot::Phase::radditzSift:
                        oss << ",r_cols_left=" << static_cast<std::uint64_t>(s.radditz_cols_remaining)
                            << ",r_passes=" << static_cast<std::uint64_t>(s.radditz_passes_last);
                        break;
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
                oss << '[' << ts_ms << "] phase=init";
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
