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
#include <format>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include <string>
#include <vector>
#include "decompress/Block/detail/rows_match_lsm.h"
#include "decompress/CrossSum/LateralSumMatrix.h"
#include "decompress/CrossSum/VerticalSumMatrix.h"
#include "decompress/Phases/RadditzSift/RadditzSift.h"
#include "common/O11y/O11y.h"
#include "decompress/Block/detail/SnapshotGuard.h"

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
                                      const ::crsce::decompress::xsum::LateralSumMatrix &lsm,
                                      const ::crsce::decompress::xsum::VerticalSumMatrix &vsm) {
        constexpr std::size_t S = Csm::kS;
        snap.phase = BlockSolveSnapshot::Phase::radditzSift;
        snap.radditz_kind = 1; // VSM-focused Radditz
        ::crsce::o11y::O11y::instance().event("radditz_sift_start");
        snap.radditz_status = 1; // started
        const ::crsce::decompress::detail::SnapshotGuard _pub(snap);
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
            if (have != static_cast<std::size_t>(vsm.targets()[c])) { ok = false; break; }
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
                    if (have != static_cast<std::size_t>(vsm.targets()[c])) { bad.push_back(c); if (bad.size() >= 16) { break; } }
                }
                std::string sample;
                for (std::size_t i = 0; i < bad.size(); ++i) {
                    const std::size_t c = bad[i];
                    const auto have = static_cast<std::size_t>(csm.count_vsm(c));
                    if (i) { sample += ", "; }
                    sample += std::format("(c={},have={},vsm={})", c, have,
                                          static_cast<std::size_t>(vsm.targets()[c]));
                }
                std::print(stderr, "radditz: failed cols={} sample={} (seed_belief={})\n",
                           static_cast<std::uint64_t>(bad.size()), sample,
                           static_cast<std::uint64_t>(snap.rng_seed_belief));
            } catch (...) { /* ignore */ }
        }
        return ok; // guard publishes
    }
}
