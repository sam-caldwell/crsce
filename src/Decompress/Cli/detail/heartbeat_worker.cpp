/**
 * @file heartbeat_worker.cpp
 * @brief Implementation of decompressor CLI heartbeat thread entrypoint.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#include "decompress/Cli/detail/heartbeat_worker.h"

#include <atomic>
#include <chrono>
#include <print>
#include <thread>
#include <cstdint>
#include <cstdio>

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
        while (run_flag && run_flag->load(std::memory_order_relaxed)) {
            const auto now = system_clock::now();
            const auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count();
            const auto ts_ms = static_cast<std::int64_t>(ms);
            const auto snap_opt = ::crsce::decompress::get_block_solve_snapshot();
            if (snap_opt) {
                const auto &s = *snap_opt;
                const char *ph = ::crsce::decompress::detail::phase_to_cstr(s.phase);
                std::print("[{}] phase={} solved={} unknown={}",
                           ts_ms, ph,
                           static_cast<std::uint64_t>(s.solved),
                           static_cast<std::uint64_t>(s.unknown_total));
                switch (s.phase) {
                    case ::crsce::decompress::BlockSolveSnapshot::Phase::de:
                        std::print(",de_status={}", s.de_status);
                        break;
                    case ::crsce::decompress::BlockSolveSnapshot::Phase::rowPhase:
                        std::print(",rows_it={},row_status={}",
                                   static_cast<std::uint64_t>(s.row_phase_iterations),
                                   s.bitsplash_status);
                        break;
                    case ::crsce::decompress::BlockSolveSnapshot::Phase::radditzSift:
                        std::print(",r_cols_left={},r_passes={}",
                                   static_cast<std::uint64_t>(s.radditz_cols_remaining),
                                   static_cast<std::uint64_t>(s.radditz_passes_last));
                        break;
                    case ::crsce::decompress::BlockSolveSnapshot::Phase::gobp:
                        std::print(",gobp_status={}", s.gobp_status);
                        break;
                    default:
                        break;
                }
                std::print("\n");
            } else {
                std::print("[{}] phase=init\n", ts_ms);
            }
            std::fflush(stdout);
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }
    }
}
