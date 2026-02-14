/**
 * @file execute_radditz_dsm_sift_and_validate.cpp
 * @brief DSM-focused 2x2 swap sift to reduce diagonal deficits while preserving row/col.
 */
#include "decompress/Block/detail/execute_radditz_dsm_sift_and_validate.h"

#include <vector>
#include <cstddef>
#include <cstdint>
#include <span>

#include "decompress/Utils/detail/index_calc_d.h"
#include "decompress/Utils/detail/index_calc_x.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/get_block_solve_snapshot.h"
#include <cstdlib>

namespace {
    // Local helper in anonymous namespace to satisfy clang-tidy's internal linkage guidance
    bool dsm_accept_swap(const std::size_t dx_before, const std::size_t dx_after,
                         const int d_sat_lost) {
        if (dx_after > dx_before) { return false; }
        int lock_dsm = 0;
        if (const auto snap = crsce::decompress::get_block_solve_snapshot(); snap.has_value()) { lock_dsm = snap->lock_dsm_sat; }
        if (lock_dsm && d_sat_lost > 0) { return false; }
        // No LH validation during DSM sift
        return true;
    }
}

namespace crsce::decompress::detail {
    bool execute_radditz_dsm_sift_and_validate(Csm &csm,
                                               ConstraintState &st,
                                               BlockSolveSnapshot &snap,
                                               const std::span<const std::uint16_t> dsm,
                                               const std::span<const std::uint8_t> lh) {
        (void)lh; // no LH validation during DSM sift
        using crsce::decompress::detail::calc_d;
        using crsce::decompress::detail::calc_x;
        constexpr std::size_t S = Csm::kS;
        // No LH validation during DSM sift

        // Ensure heartbeat shows we are in DSM-focused Radditz stage
        snap.phase = BlockSolveSnapshot::Phase::radditzSift;
        snap.radditz_kind = 2;
        // Build current diag/xdiag counts
        std::vector<std::uint16_t> dcnt(S, 0);
        std::vector<std::uint16_t> xcnt(S, 0);
        for (std::size_t r = 0; r < S; ++r) {
            for (std::size_t c = 0; c < S; ++c) {
                if (csm.get(r, c)) {
                    ++dcnt[calc_d(r, c)];
                    ++xcnt[calc_x(r, c)];
                }
            }
        }

        unsigned max_passes = 2U;
        if (const char *p = std::getenv("CRSCE_DSF_MAX_PASSES")) { // NOLINT(concurrency-mt-unsafe)
            const std::int64_t v = std::strtoll(p, nullptr, 10);
            if (v > 0) { max_passes = static_cast<unsigned>(v); }
        }
        unsigned trials_per_def = 12U;
        if (const char *p = std::getenv("CRSCE_DSF_TRIALS_PER_DEFICIT")) { // NOLINT(concurrency-mt-unsafe)
            const std::int64_t v = std::strtoll(p, nullptr, 10);
            if (v > 0) { trials_per_def = static_cast<unsigned>(v); }
        }

        auto diag_err = [&](std::size_t d, std::uint16_t cnt){ const std::uint16_t tgt = (d<dsm.size())?dsm[d]:0; return (cnt>tgt)?(cnt-tgt):(tgt-cnt); };

        for (unsigned pass=0; pass<max_passes; ++pass) {
            std::size_t accepts = 0;
            // Simple deterministic rectangle sampling favoring DSM cost decrease
            for (std::size_t s = 0; s < static_cast<std::size_t>(S*S) && accepts < 32; ++s) {
                const std::size_t r0 = (s * 73U) % S;
                const std::size_t r1 = (r0 + 1U + ((s * 97U) % (S - 1U))) % S;
                const std::size_t c0 = (s * 89U) % S;
                const std::size_t c1 = (c0 + 1U + ((s * 131U) % (S - 1U))) % S;
                if (r0==r1 || c0==c1) { continue; }
                const bool b00 = csm.get(r0,c0); const bool b11 = csm.get(r1,c1);
                const bool b01 = csm.get(r0,c1); const bool b10 = csm.get(r1,c0);
                const bool main_rect = (b00 && b11 && !b01 && !b10);
                const bool off_rect  = (!b00 && !b11 && b01 && b10);
                if (!main_rect && !off_rect) { continue; }
                // Skip locked cells
                if ((main_rect && (csm.is_locked(r0,c0) || csm.is_locked(r1,c1) || csm.is_locked(r0,c1) || csm.is_locked(r1,c0))) ||
                    (off_rect  && (csm.is_locked(r0,c1) || csm.is_locked(r1,c0) || csm.is_locked(r0,c0) || csm.is_locked(r1,c1)))) {
                    continue;
                }
                const std::size_t d00 = calc_d(r0,c0);
                const std::size_t d11 = calc_d(r1,c1);
                const std::size_t d01 = calc_d(r0,c1);
                const std::size_t d10 = calc_d(r1,c0);
                // Proposed diag counts after swap
                const std::size_t d00_n = dcnt[d00] + (main_rect ? -1 : +1);
                const std::size_t d11_n = dcnt[d11] + (main_rect ? -1 : +1);
                const std::size_t d01_n = dcnt[d01] + (main_rect ? +1 : -1);
                const std::size_t d10_n = dcnt[d10] + (main_rect ? +1 : -1);
                const std::size_t cost_d_before = diag_err(d00, dcnt[d00]) + diag_err(d11, dcnt[d11]) + diag_err(d01, dcnt[d01]) + diag_err(d10, dcnt[d10]);
                const std::size_t cost_d_after  = diag_err(d00, d00_n) + diag_err(d11, d11_n) + diag_err(d01, d01_n) + diag_err(d10, d10_n);
                if (cost_d_after > cost_d_before) { continue; }
                // Satisfaction counts lost/gained (diag only)
                const bool d00_sat_b = (dcnt[d00] == dsm[d00]);
                const bool d11_sat_b = (dcnt[d11] == dsm[d11]);
                const bool d01_sat_b = (dcnt[d01] == dsm[d01]);
                const bool d10_sat_b = (dcnt[d10] == dsm[d10]);
                const bool d00_sat_a = (d00_n == dsm[d00]);
                const bool d11_sat_a = (d11_n == dsm[d11]);
                const bool d01_sat_a = (d01_n == dsm[d01]);
                const bool d10_sat_a = (d10_n == dsm[d10]);
                const int d_sat_lost = (d00_sat_b && !d00_sat_a) + (d11_sat_b && !d11_sat_a) + (d01_sat_b && !d01_sat_a) + (d10_sat_b && !d10_sat_a);

                // Apply acceptance (DSM only + LH guard when breaking)
                if (!dsm_accept_swap(cost_d_before, cost_d_after, d_sat_lost)) {
                    continue;
                }
                // Commit
                if (main_rect) { csm.put(r0,c0,false); csm.put(r1,c1,false); csm.put(r0,c1,true); csm.put(r1,c0,true); }
                else { csm.put(r0,c1,false); csm.put(r1,c0,false); csm.put(r0,c0,true); csm.put(r1,c1,true); }
                dcnt[d00]=d00_n; dcnt[d11]=d11_n; dcnt[d01]=d01_n; dcnt[d10]=d10_n;
                // Update residuals for touched diags only
                // Update residuals explicitly for touched diags
                {
                    const std::uint16_t tgt = dsm[d00]; const std::uint16_t cnt = dcnt[d00]; const std::uint16_t U = st.U_diag.at(d00);
                    const std::uint16_t need = (tgt>cnt)?static_cast<std::uint16_t>(tgt-cnt):0U; st.R_diag.at(d00) = (need>U)?U:need;
                }
                {
                    const std::uint16_t tgt = dsm[d11]; const std::uint16_t cnt = dcnt[d11]; const std::uint16_t U = st.U_diag.at(d11);
                    const std::uint16_t need = (tgt>cnt)?static_cast<std::uint16_t>(tgt-cnt):0U; st.R_diag.at(d11) = (need>U)?U:need;
                }
                {
                    const std::uint16_t tgt = dsm[d01]; const std::uint16_t cnt = dcnt[d01]; const std::uint16_t U = st.U_diag.at(d01);
                    const std::uint16_t need = (tgt>cnt)?static_cast<std::uint16_t>(tgt-cnt):0U; st.R_diag.at(d01) = (need>U)?U:need;
                }
                {
                    const std::uint16_t tgt = dsm[d10]; const std::uint16_t cnt = dcnt[d10]; const std::uint16_t U = st.U_diag.at(d10);
                    const std::uint16_t need = (tgt>cnt)?static_cast<std::uint16_t>(tgt-cnt):0U; st.R_diag.at(d10) = (need>U)?U:need;
                }
                ++accepts;
                if (accepts >= trials_per_def) { break; }
            }
            if (accepts == 0) { break; }
        }

        // Validate DSM satisfaction (best-effort): true if all dcnt[i]==dsm[i]
        bool ok = true; for (std::size_t i=0;i<S && i<dsm.size();++i) { if (dcnt[i] != dsm[i]) { ok=false; break; } }
        // Snapshot: lock DSM if desired will be set by caller; just publish residuals snapshot
        snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
        return ok;
    }
}
