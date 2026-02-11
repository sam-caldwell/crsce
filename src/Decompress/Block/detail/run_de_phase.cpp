/**
 * @file run_de_phase.cpp
 */
#include "decompress/Block/detail/run_de_phase.h"
#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>
#include <string>
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Csm/detail/Csm.h"

#include <chrono>
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "decompress/Block/detail/pre_polish_commit_valid_prefix.h"
#include "common/O11y/event.h"
#include "common/O11y/metric_i64.h"

namespace crsce::decompress::detail {
    bool run_de_phase(DeterministicElimination &det,
                      ConstraintState &st,
                      Csm &csm,
                      BlockSolveSnapshot &snap,
                      const std::span<const std::uint8_t> lh,
                      const int max_iters) {
        constexpr std::size_t S = Csm::kS;
        snap.de_status = 1;
        Csm baseline_csm = csm; // localized baseline for prefix commits during DE
        ConstraintState baseline_st = st;
        for (int iter = 0; iter < max_iters; ++iter) {
            std::size_t progress = 0;
            try {
                snap.phase = BlockSolveSnapshot::Phase::de;
                const auto t0 = std::chrono::steady_clock::now();
                const std::size_t v = det.solve_step();
                const auto t1 = std::chrono::steady_clock::now();
                progress += v;
                snap.time_de_ms += static_cast<std::size_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
            } catch (const std::exception &e) {
                snap.iter = static_cast<std::size_t>(iter);
                snap.message = e.what();
                snap.de_status = 2; // failed
                snap.U_row.assign(st.U_row.begin(), st.U_row.end());
                snap.U_col.assign(st.U_col.begin(), st.U_col.end());
                snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
                snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
                std::size_t sumU = 0; for (const auto u: st.U_row) { sumU += static_cast<std::size_t>(u); }
                snap.unknown_total = sumU;
                snap.solved = (S * S) - sumU;
                set_block_solve_snapshot(snap);
                return false;
            }
            snap.iter = static_cast<std::size_t>(iter);
            snap.U_row.assign(st.U_row.begin(), st.U_row.end());
            snap.U_col.assign(st.U_col.begin(), st.U_col.end());
            snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
            snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
            {
                std::size_t sumU = 0; for (const auto u: st.U_row) { sumU += static_cast<std::size_t>(u); }
                snap.unknown_total = sumU;
                snap.solved = (S * S) - sumU;
                if ((iter % 250) == 0) { ::crsce::o11y::metric("de_progress", static_cast<std::int64_t>(snap.solved)); }
            }
            set_block_solve_snapshot(snap);

            // Prefix gating within DE: commit any valid prefix and allow a short DE propagate
            {
                const auto t0_lh = std::chrono::steady_clock::now();
                const bool anyp = ::crsce::decompress::detail::commit_valid_prefix(
                    csm, st, lh, baseline_csm, baseline_st, snap, /*rs*/0);
                const auto t1_lh = std::chrono::steady_clock::now();
                snap.time_lh_ms += static_cast<std::size_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(t1_lh - t0_lh).count());
                if (anyp) {
                    for (int it = 0; it < 50 && !det.solved(); ++it) {
                        const auto t0 = std::chrono::steady_clock::now();
                        const auto vv = det.solve_step();
                        const auto t1 = std::chrono::steady_clock::now();
                        snap.time_de_ms += static_cast<std::size_t>(
                            std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
                        if (vv == 0) { break; }
                    }
                }
            }
            if (det.solved()) {
                snap.de_status = 3; // completed
                ::crsce::o11y::event("block_terminating", {{"reason", std::string("possible_solution")}});
                break;
            }
            if (progress == 0) {
                ::crsce::o11y::event("de_steady_state");
                ::crsce::o11y::metric("de_to_gobp", 1LL);
                snap.de_status = 3; // completed DE stage
                break;
            }
            ::crsce::o11y::metric("block_iter_end", static_cast<std::int64_t>(iter),
                                  {{"max", std::to_string(max_iters)}, {"progress", std::to_string(progress)}});
        }
        return true;
    }
}
