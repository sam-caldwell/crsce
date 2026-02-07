/**
 * @file run_final_backtrack_worker.cpp
 * @brief Definition for final boundary backtrack worker.
 */
#include "decompress/Block/detail/run_final_backtrack_worker.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"

namespace crsce::decompress::detail {

/**
 * @name run_final_backtrack_worker
 * @brief Worker for final single-cell boundary backtrack over top-K candidates.
 */
void run_final_backtrack_worker(std::size_t wi, // NOLINT(misc-use-internal-linkage)
                                const std::vector<BTTaskPair> &tasks,
                                std::atomic<std::size_t> &next_idx,
                                std::atomic<bool> &found,
                                const std::span<const std::uint8_t> lh,
                                const std::size_t boundary,
                                const std::size_t valid_now,
                                const std::size_t S,
                                const Csm &csm_in,
                                const ConstraintState &st_in,
                                std::mutex &adopt_mu,
                                Csm &c_winner,
                                ConstraintState &st_winner,
                                BlockSolveSnapshot::ThreadEvent &ev_out) {
    const auto start = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch()).count());
    std::string outcome = "rejected";
    while (!found.load(std::memory_order_relaxed)) {
        const std::size_t idx = next_idx.fetch_add(1, std::memory_order_relaxed);
        if (idx >= tasks.size()) { break; }
        const BTTaskPair t = tasks[idx];
        Csm c_try = csm_in; ConstraintState st_try = st_in;
        const std::size_t c_pick = t.first;
        const bool assume_one = t.second;
        c_try.put(boundary, c_pick, assume_one);
        c_try.lock(boundary, c_pick);
        if (st_try.U_row.at(boundary) > 0) { --st_try.U_row.at(boundary); }
        if (st_try.U_col.at(c_pick) > 0) { --st_try.U_col.at(c_pick); }
        const std::size_t d0 = (c_pick >= boundary) ? (c_pick - boundary) : (c_pick + S - boundary);
        const std::size_t x0 = (boundary + c_pick) % S;
        if (st_try.U_diag.at(d0) > 0) { --st_try.U_diag.at(d0); }
        if (st_try.U_xdiag.at(x0) > 0) { --st_try.U_xdiag.at(x0); }
        if (assume_one) {
            if (st_try.R_row.at(boundary) > 0) { --st_try.R_row.at(boundary); }
            if (st_try.R_col.at(c_pick) > 0) { --st_try.R_col.at(c_pick); }
            if (st_try.R_diag.at(d0) > 0) { --st_try.R_diag.at(d0); }
            if (st_try.R_xdiag.at(x0) > 0) { --st_try.R_xdiag.at(x0); }
        }
        DeterministicElimination det_bt{c_try, st_try};
        static constexpr int bt_iters = 6000;
        for (int it = 0; it < bt_iters; ++it) {
            const std::size_t prog = det_bt.solve_step();
            if (st_try.U_row.at(boundary) == 0 || prog == 0) { break; }
        }
        if (st_try.U_row.at(boundary) == 0) {
            const std::size_t check_rows = std::min<std::size_t>(valid_now + 1, S);
            const RowHashVerifier verifier_try;
            if (check_rows > 0 && verifier_try.verify_rows(c_try, lh, check_rows)) {
                bool expected = false;
                if (found.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                    const std::scoped_lock lk(adopt_mu);
                    c_winner = c_try; st_winner = st_try; outcome = "adopted";
                }
            }
        }
    }
    const auto stop = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch()).count());
    ev_out.name = std::string("btw_total_") + std::to_string(wi);
    ev_out.start_ms = start; ev_out.stop_ms = stop; ev_out.outcome = outcome;
}

} // namespace crsce::decompress::detail
