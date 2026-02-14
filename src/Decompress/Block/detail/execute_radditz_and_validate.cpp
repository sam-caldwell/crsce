/**
 * @file execute_radditz_and_validate.cpp
 * @brief Execute Radditz Sift (column-focused swaps) and validate VSM + row invariant.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Block/detail/execute_radditz_and_validate.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <print>
#include <span>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include <string>
#include <vector>
#include "decompress/Block/detail/rows_match_lsm.h"
#include "decompress/Phases/RadditzSift/RadditzSift.h"
#include "common/O11y/event.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"

namespace crsce::decompress::detail {
    /**
     * @name execute_radditz_and_validate
     * @brief Run the Radditz Sift phase to reduce column deficits and ensure row sums remain intact.
     * @param csm Cross‑Sum Matrix to update (bits/locks).
     * @param st Constraint state (R/U) updated in place.
     * @param snap Snapshot for timings, counters, and status codes.
     * @param lsm Row targets to re‑validate after sifting (size ≥ Csm::kS).
     * @param vsm Column targets to validate after sifting (size ≥ Csm::kS).
     * @return bool True if both columns satisfy VSM and rows still match LSM; false otherwise.
     */
    bool execute_radditz_and_validate(Csm &csm,
                                      ConstraintState &st,
                                      BlockSolveSnapshot &snap,
                                      const std::span<const std::uint16_t> lsm,
                                      std::span<const std::uint16_t> vsm) {
        constexpr std::size_t S = Csm::kS;
        snap.phase = BlockSolveSnapshot::Phase::radditzSift;
        snap.radditz_kind = 1; // VSM-focused Radditz
        ::crsce::o11y::event("radditz_sift_start");
        snap.radditz_status = 1; // started
        set_block_solve_snapshot(snap);
        const std::size_t rsf = ::crsce::decompress::phases::radditz_sift_phase(csm, st, snap, 0);
        (void)rsf; // no DE settle here
        snap.U_row.assign(st.U_row.begin(), st.U_row.end());
        snap.U_col.assign(st.U_col.begin(), st.U_col.end());
        snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
        snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
        std::size_t sumU = 0; for (const auto u : st.U_row) { sumU += static_cast<std::size_t>(u); }
        snap.unknown_total = sumU;
        snap.solved = (S * S) - sumU;

        bool ok = true;
        for (std::size_t c = 0; c < S; ++c) {
            const auto have = static_cast<std::size_t>(csm.count_vsm(c));
            if (have != static_cast<std::size_t>(vsm[c])) { ok = false; break; }
        }
        if (ok && !rows_match_lsm(csm, lsm)) {
            ok = false;
            snap.message = "radditz violated row-sum invariant";
        }
        snap.radditz_status = ok ? 3 : 2;
        if (!ok) {
            try {
                std::vector<std::size_t> bad; bad.reserve(16);
                for (std::size_t c = 0; c < S; ++c) {
                    const auto have = static_cast<std::size_t>(csm.count_vsm(c));
                    if (have != static_cast<std::size_t>(vsm[c])) { bad.push_back(c); if (bad.size() >= 16) { break; } }
                }
                std::string sample;
                for (std::size_t i = 0; i < bad.size(); ++i) {
                    const std::size_t c = bad[i];
                    const auto have = static_cast<std::size_t>(csm.count_vsm(c));
                    sample += "(c=" + std::to_string(c) + ",have=" + std::to_string(have) + ",vsm=" + std::to_string(vsm[c]) + ")";
                    if (i + 1U < bad.size()) { sample += ", "; }
                }
                std::print(stderr, "radditz: failed cols={} sample={} (seed_belief={})\n",
                           static_cast<std::uint64_t>(bad.size()), sample,
                           static_cast<std::uint64_t>(snap.rng_seed_belief));
            } catch (...) { /* ignore */ }
        }
        set_block_solve_snapshot(snap);
        return ok;
    }
}
