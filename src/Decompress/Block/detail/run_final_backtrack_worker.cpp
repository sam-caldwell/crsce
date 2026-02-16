/**
 * @file run_final_backtrack_worker.cpp
 * @brief Definition for final boundary backtrack worker.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Block/detail/run_final_backtrack_worker.h"
#include "decompress/Block/detail/BTTaskPair.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <format>
#include <vector>

#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Utils/detail/now_ms.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Phases/DeterministicElimination/DeterministicElimination.h"

namespace crsce::decompress::detail {

/**
 * @name run_final_backtrack_worker
 * @brief Worker for final single-cell boundary backtrack over top-K candidates.
 * @param wi Worker index for naming the event sample.
 * @param tasks Ordered candidate list of column,assume_one pairs.
 * @param next_idx Atomic cursor across tasks shared by workers.
 * @param found Atomic flag indicating a successful adoption occurred.
 * @param lh Span of LH digest bytes for verification.
 * @param boundary Current boundary row index.
 * @param valid_now Current verified prefix length.
 * @param S CSM dimension (rows/cols).
 * @param csm_in Baseline CSM to clone from for each task.
 * @param st_in Baseline constraint state to clone from for each task.
 * @param adopt_mu Mutex used to guard winner adoption into output.
 * @param c_winner Out: adopted CSM result if successful.
 * @param st_winner Out: adopted state result if successful.
 * @param ev_out Out: thread event metadata populated with timing/outcome.
 * @return void
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
    const auto start = now_ms();
    std::string outcome = "rejected";
    while (!found.load(std::memory_order_relaxed)) {
        const std::size_t idx = next_idx.fetch_add(1, std::memory_order_relaxed);
        if (idx >= tasks.size()) { break; }
        const BTTaskPair t = tasks[idx];
        Csm c_try = csm_in; ConstraintState st_try = st_in;
        const std::size_t c_pick = t.first;
        const bool assume_one = t.second;
        if (assume_one) { c_try.set(boundary, c_pick); } else { c_try.clear(boundary, c_pick); }
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
        BlockSolveSnapshot dummy_snap{S, st_try, {}, {}, {}, {}, 0ULL};
        const std::span<const std::uint8_t> empty_lh{};
        static constexpr std::uint64_t bt_iters = 6000ULL;
        DeterministicElimination det_bt{bt_iters, c_try, st_try, dummy_snap, empty_lh};
        for (std::uint64_t it = 0; it < bt_iters; ++it) {
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
    const auto stop = now_ms();
    ev_out.name = std::format("btw_total_{}", wi);
    ev_out.start_ms = start; ev_out.stop_ms = stop; ev_out.outcome = outcome;
}

} // namespace crsce::decompress::detail
