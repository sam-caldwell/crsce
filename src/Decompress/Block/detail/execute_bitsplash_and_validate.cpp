/**
 * @file execute_bitsplash_and_validate.cpp
 * @brief Execute BitSplash row phase and validate row-sum completion against LSM.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Block/detail/execute_bitsplash_and_validate.h"

#include <cstddef>
#include <cstdint> // NOLINT
#include <cstdio>
#include <span>
#include <print>
#include <format>
#include <string>
#include <vector>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Phases/BitSplash/BitSplash.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "common/O11y/O11y.h"

namespace crsce::decompress::detail {
    /**
     * @name execute_bitsplash_and_validate
     * @brief Run BitSplash to satisfy row targets and update the snapshot; return false on mismatch.
     * @param csm Cross‑Sum Matrix to update (bits/locks).
     * @param st Constraint state (R/U) updated in place.
     * @param snap Snapshot for timings, counters, and status codes.
     * @param lsm Row target counts to validate against (size ≥ Csm::kS).
     * @return bool True if every row count matches `lsm`; false otherwise.
     */
    bool execute_bitsplash_and_validate(Csm &csm,
                                        ConstraintState &st,
                                        BlockSolveSnapshot &snap,
                                        const std::span<const std::uint16_t> lsm) {
        constexpr std::size_t S = Csm::kS;
        snap.phase = BlockSolveSnapshot::Phase::rowPhase;
        ::crsce::o11y::O11y::instance().event("bitsplash_start");
        snap.bitsplash_status = 1; // started
        set_block_solve_snapshot(snap);
        (void) ::crsce::decompress::phases::bit_splash(csm, st, snap, 0);
        snap.U_row.assign(st.U_row.begin(), st.U_row.end());
        snap.U_col.assign(st.U_col.begin(), st.U_col.end());
        snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
        snap.U_xdiag.assign(st.U_xdiag.begin(), st.U_xdiag.end());
        std::size_t sumU = 0;
        for (const auto u: st.U_row) {
            sumU += static_cast<std::size_t>(u);
        }
        snap.unknown_total = sumU;
        snap.solved = (S * S) - sumU;

        bool complete = true;
        for (std::size_t r = 0; r < S; ++r) {
            std::size_t ones = 0;
            for (std::size_t c = 0; c < S; ++c) {
                if (csm.get(r, c)) { ++ones; }
            }
            if (ones != static_cast<std::size_t>(lsm[r])) {
                complete = false;
                break;
            }
        }
        snap.bitsplash_status = complete ? 3 : 2;
        if (!complete) {
            try {
                std::vector<std::size_t> bad;
                bad.reserve(16);
                for (std::size_t r = 0; r < S; ++r) {
                    std::size_t ones = 0;
                    for (std::size_t c = 0; c < S; ++c) { if (csm.get(r, c)) { ++ones; } }
                    if (ones != static_cast<std::size_t>(lsm[r])) {
                        bad.push_back(r);
                        if (bad.size() >= 16) { break; }
                    }
                }
                std::string sample;
                for (std::size_t i = 0; i < bad.size(); ++i) {
                    const std::size_t r = bad[i];
                    std::size_t ones = 0;
                    for (std::size_t c = 0; c < S; ++c) {
                        if (csm.get(r, c)) { ++ones; }
                    }
                    if (i) { sample += ", "; }
                    sample += std::format("(r={},have={},lsm={})", r, ones,
                                          static_cast<std::size_t>(lsm[r]));
                }
                std::print(stderr, "bitsplash: failed rows={} sample={} (seed_belief={})\n",
                           static_cast<std::uint64_t>(bad.size()), sample,
                           static_cast<std::uint64_t>(snap.rng_seed_belief));
            } catch (...) {
                /* ignore */
            }
            snap.message = "bitsplash rows not satisfied";
            set_block_solve_snapshot(snap);
            return false;
        }
        set_block_solve_snapshot(snap);
        return true;
    }
}
