/**
 * @file execute_radditz_dsm_sift_and_validate.cpp
 * @brief DSM-focused 2x2 swap sift to reduce diagonal deficits while preserving row/col.
 */
#include "decompress/Block/detail/execute_radditz_dsm_sift_and_validate.h"

#include <vector>
#include <cstddef>
#include <cstdint>
#include <span>
#include <algorithm>

#include "decompress/Utils/detail/index_calc_d.h"
#include "decompress/Utils/detail/index_calc_x.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "decompress/Block/detail/dsm_accept_swap.h"
#include "common/O11y/detail/now_ms.h"
#include <cstdlib>

// dsm_accept_swap moved to its own TU

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

        // Ensure heartbeat shows we are in the DSM-focused Radditz stage
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

        auto refresh_dsm_metrics = [&]() {
            std::uint16_t err_min = 0xFFFFU;
            std::uint16_t err_max = 0U;
            std::size_t sat = 0;
            for (std::size_t i = 0; i < S && i < dsm.size(); ++i) {
                const std::uint16_t cnt = dcnt[i];
                const std::uint16_t tgt = dsm[i];
                const std::uint16_t err = (cnt > tgt) ? static_cast<std::uint16_t>(cnt - tgt)
                                                       : static_cast<std::uint16_t>(tgt - cnt);
                err_min = std::min(err_min, err);
                err_max = std::max(err_max, err);
                if (cnt == tgt) { ++sat; }
            }
            if (err_min == 0xFFFFU) { err_min = 0U; }
            snap.dsm_err_min = err_min;
            snap.dsm_err_max = err_max;
            snap.dsm_avg_pct = static_cast<int>((sat * 100ULL) / (S ? S : 1ULL));
        };
        // Initialize metrics before sifting
        refresh_dsm_metrics();
        ::crsce::decompress::set_block_solve_snapshot(snap);
        // Time gating for snapshot publishing
        auto now_ms = []() -> std::uint64_t { return ::crsce::o11y::detail::now_ms(); };
        std::uint64_t last_pub_ms = now_ms();
        auto maybe_publish = [&](bool force=false) {
            const std::uint64_t t = now_ms();
            if (force || (t - last_pub_ms) >= 2000ULL) {
                refresh_dsm_metrics();
                ::crsce::decompress::set_block_solve_snapshot(snap);
                last_pub_ms = t;
            }
        };

        unsigned max_passes = 2U;
        if (const char *p = std::getenv("CRSCE_DSF_MAX_PASSES")) { // NOLINT(concurrency-mt-unsafe)
            if (const std::int64_t v = std::strtoll(p, nullptr, 10); v > 0) {
                max_passes = static_cast<unsigned>(v);
            }
        }
        unsigned trials_per_def = 12U;
        if (const char *p = std::getenv("CRSCE_DSF_TRIALS_PER_DEFICIT")) { // NOLINT(concurrency-mt-unsafe)
            if (const std::int64_t v = std::strtoll(p, nullptr, 10); v > 0) {
                trials_per_def = static_cast<unsigned>(v);
            }
        }

        auto diag_err = [&](const std::size_t d, const std::uint16_t cnt) {
            const std::uint16_t tgt = (d<dsm.size())?dsm[d]:0; return (cnt>tgt)?(cnt-tgt):(tgt-cnt);
        };

        for (unsigned pass=0; pass<max_passes; ++pass) {
            std::size_t accepts = 0;
            // Guided phase: bias proposals from surplus diagonals to deficit diagonals
            {
                // Build surplus/deficit lists by error magnitude
                struct Ent { std::uint16_t err; std::uint16_t idx; };
                std::vector<Ent> surplus; surplus.reserve(S);
                std::vector<Ent> deficit; deficit.reserve(S);
                for (std::size_t i = 0; i < S && i < dsm.size(); ++i) {
                    const std::uint16_t cnt = dcnt[i];
                    if (const std::uint16_t tgt = dsm[i]; cnt > tgt) {
                        surplus.push_back(Ent{ .err = static_cast<std::uint16_t>(cnt - tgt), .idx = static_cast<std::uint16_t>(i) });
                    } else if (cnt < tgt) {
                        deficit.push_back(Ent{ .err = static_cast<std::uint16_t>(tgt - cnt), .idx = static_cast<std::uint16_t>(i) });
                    }
                }
                auto by_err_desc = [](const Ent &a, const Ent &b) {
                    return a.err > b.err;
                };
                std::ranges::sort(surplus, by_err_desc);
                std::ranges::sort(deficit, by_err_desc);

                const std::size_t Kp = std::min<std::size_t>(surplus.size(), 32);
                const std::size_t Km = std::min<std::size_t>(deficit.size(), 32);

                // Quick deficit membership bitmap (top-K only)
                std::vector<std::uint8_t> is_def(S, 0);
                for (std::size_t i = 0; i < Km; ++i) {
                    is_def[deficit[i].idx] = 1U;
                }

                // Try top-K surplus pairs and small Δr set; sample rows sparsely per pair
                for (std::size_t ia = 0; ia < Kp && accepts < trials_per_def; ++ia) {
                    for (std::size_t ib = ia + 1; ib < Kp && accepts < trials_per_def; ++ib) {
                        const std::size_t da = surplus[ia].idx;
                        const std::size_t db = surplus[ib].idx;

                        // Search a small set of Δr values that map increments into deficits
                        for (std::size_t dr_try = 1; dr_try < 64 && accepts < trials_per_def; ++dr_try) {

                            const std::size_t d01 = (db + dr_try) % S;
                            if (const std::size_t d10 = (da + (S - (dr_try % S))) % S; !(is_def[d01] && is_def[d10])) {
                                continue;
                            }
                            // Sample r0 sparsely with a stride coprime-ish to S
                            std::size_t tried = 0;
                            for (std::size_t r0_seed = 0; r0_seed < S && accepts < trials_per_def && tried < 32; r0_seed += 17) {
                                const std::size_t r0 = r0_seed % S;
                                const std::size_t r1 = (r0 + dr_try) % S;
                                const std::size_t c0 = (da + r0) % S;
                                const std::size_t c1 = (db + r1) % S;
                                // Canonicalize rectangle
                                const std::size_t rr0 = (r0 < r1) ? r0 : r1;
                                const std::size_t rr1 = (r0 < r1) ? r1 : r0;
                                const std::size_t cc0 = (c0 < c1) ? c0 : c1;
                                const std::size_t cc1 = (c0 < c1) ? c1 : c0;
                                ++snap.dsm_rectangles_chosen;
                                if ((snap.dsm_rectangles_chosen & 0xFFFFULL) == 0ULL) { // every 65536 rectangles
                                    maybe_publish(false);
                                }
                                // Pattern + locks
                                const bool b00 = csm.get(rr0,cc0); const bool b11 = csm.get(rr1,cc1);
                                const bool b01 = csm.get(rr0,cc1); const bool b10 = csm.get(rr1,cc0);
                                const bool main_rect = (b00 && b11 && !b01 && !b10);
                                const bool off_rect  = (!b00 && !b11 && b01 && b10);

                                if (!main_rect && !off_rect) {
                                    ++tried; continue;
                                }

                                const auto is_csm_locked = [&](
                                    const std::size_t a, const std::size_t b, const std::size_t c, const std::size_t d) {
                                    return csm.is_locked(a,b) || csm.is_locked(c,d) ||
                                           csm.is_locked(a,d) || csm.is_locked(c,b);
                                };

                                if ((main_rect && is_csm_locked(rr0,cc0,rr1,cc1) ) || (off_rect  && is_csm_locked(rr0,cc1,rr1,cc0))) {
                                    ++tried; continue;
                                }

                                // compute local dsm indexes
                                const std::size_t d00 = calc_d(rr0,cc0);
                                const std::size_t d11 = calc_d(rr1,cc1);
                                const std::size_t d01b = calc_d(rr0,cc1);
                                const std::size_t d10b = calc_d(rr1,cc0);

                                // Compute local DSM deltas
                                const std::size_t d00_n = dcnt[d00] + (main_rect ? -1 : +1);
                                const std::size_t d11_n = dcnt[d11] + (main_rect ? -1 : +1);
                                const std::size_t d01_n = dcnt[d01b] + (main_rect ? +1 : -1);
                                const std::size_t d10_n = dcnt[d10b] + (main_rect ? +1 : -1);

                                const std::size_t cost_b = diag_err(d00, dcnt[d00])
                                                         + diag_err(d11, dcnt[d11])
                                                         + diag_err(d01b, dcnt[d01b])
                                                         + diag_err(d10b, dcnt[d10b]);

                                const std::size_t cost_a = diag_err(d00, d00_n)
                                                         + diag_err(d11, d11_n)
                                                         + diag_err(d01b, d01_n)
                                                         + diag_err(d10b, d10_n);

                                if (cost_a > cost_b) { ++tried; continue; }

                                // Satisfaction loss guard when locked
                                const bool d00_sat_b = (dcnt[d00] == dsm[d00]);
                                const bool d11_sat_b = (dcnt[d11] == dsm[d11]);
                                const bool d01_sat_b = (dcnt[d01b] == dsm[d01b]);
                                const bool d10_sat_b = (dcnt[d10b] == dsm[d10b]);
                                const bool d00_sat_a = (d00_n == dsm[d00]);
                                const bool d11_sat_a = (d11_n == dsm[d11]);
                                const bool d01_sat_a = (d01_n == dsm[d01b]);
                                const bool d10_sat_a = (d10_n == dsm[d10b]);

                                const int d_sat_lost = (d00_sat_b && !d00_sat_a) + (d11_sat_b && !d11_sat_a) + (d01_sat_b && !d01_sat_a) + (d10_sat_b && !d10_sat_a);
                                if (!::crsce::decompress::detail::dsm_accept_swap(cost_b, cost_a, d_sat_lost)) { ++tried; continue; }
                                // Commit
                                if (main_rect) { csm.put(rr0,cc0,false); csm.put(rr1,cc1,false); csm.put(rr0,cc1,true); csm.put(rr1,cc0,true); }
                                else { csm.put(rr0,cc1,false); csm.put(rr1,cc0,false); csm.put(rr0,cc0,true); csm.put(rr1,cc1,true); }
                                dcnt[d00]=d00_n; dcnt[d11]=d11_n; dcnt[d01b]=d01_n; dcnt[d10b]=d10_n;
                                // Update residuals
                                {
                                    const std::uint16_t tgt = dsm[d00]; const std::uint16_t cnt = dcnt[d00]; const std::uint16_t U = st.U_diag.at(d00);
                                    const std::uint16_t need = (tgt>cnt)?static_cast<std::uint16_t>(tgt-cnt):0U; st.R_diag.at(d00) = (need>U)?U:need;
                                }
                                {
                                    const std::uint16_t tgt = dsm[d11]; const std::uint16_t cnt = dcnt[d11]; const std::uint16_t U = st.U_diag.at(d11);
                                    const std::uint16_t need = (tgt>cnt)?static_cast<std::uint16_t>(tgt-cnt):0U; st.R_diag.at(d11) = (need>U)?U:need;
                                }
                                {
                                    const std::uint16_t tgt = dsm[d01b]; const std::uint16_t cnt = dcnt[d01b]; const std::uint16_t U = st.U_diag.at(d01b);
                                    const std::uint16_t need = (tgt>cnt)?static_cast<std::uint16_t>(tgt-cnt):0U; st.R_diag.at(d01b) = (need>U)?U:need;
                                }
                                {
                                    const std::uint16_t tgt = dsm[d10b]; const std::uint16_t cnt = dcnt[d10b]; const std::uint16_t U = st.U_diag.at(d10b);
                                    const std::uint16_t need = (tgt>cnt)?static_cast<std::uint16_t>(tgt-cnt):0U; st.R_diag.at(d10b) = (need>U)?U:need;
                                }
                                ++accepts;
                                ++snap.dsm_swaps;
                                snap.dsm_cost_improve_total += static_cast<std::uint64_t>(cost_b - cost_a);
                                if ((snap.dsm_swaps & 0x1FFULL) == 0ULL) { // every 512 accepts
                                    maybe_publish(false);
                                }
                                if (accepts >= trials_per_def) { break; }
                                ++tried;
                            }
                            if (accepts >= trials_per_def) { break; }
                        }
                        if (accepts >= trials_per_def) { break; }
                    }
                    if (accepts >= trials_per_def) { break; }
                }
            }
            // Fallback: Simple deterministic rectangle sampling favoring DSM cost decrease
            for (std::size_t s = 0; s < static_cast<std::size_t>(S*S) && accepts < 32; ++s) {
                const std::size_t r0_raw = (s * 73U) % S;
                const std::size_t r1_raw = (r0_raw + 1U + ((s * 97U) % (S - 1U))) % S;
                const std::size_t c0_raw = (s * 89U) % S;
                const std::size_t c1_raw = (c0_raw + 1U + ((s * 131U) % (S - 1U))) % S;
                if (r0_raw==r1_raw || c0_raw==c1_raw) { continue; }
                // Canonicalize rectangle orientation to deduplicate: r0<r1 and c0<c1
                const std::size_t rr0 = (r0_raw < r1_raw) ? r0_raw : r1_raw;
                const std::size_t rr1 = (r0_raw < r1_raw) ? r1_raw : r0_raw;
                const std::size_t cc0 = (c0_raw < c1_raw) ? c0_raw : c1_raw;
                const std::size_t cc1 = (c0_raw < c1_raw) ? c1_raw : c0_raw;
                // Count rectangle considered
                ++snap.dsm_rectangles_chosen;
                if ((snap.dsm_rectangles_chosen & 0xFFFFULL) == 0ULL) { // every 65536 rectangles
                    maybe_publish(false);
                }
                const bool b00 = csm.get(rr0,cc0); const bool b11 = csm.get(rr1,cc1);
                const bool b01 = csm.get(rr0,cc1); const bool b10 = csm.get(rr1,cc0);
                const bool main_rect = (b00 && b11 && !b01 && !b10);
                const bool off_rect  = (!b00 && !b11 && b01 && b10);
                if (!main_rect && !off_rect) { continue; }
                // Skip locked cells
                if ((main_rect && (csm.is_locked(rr0,cc0) || csm.is_locked(rr1,cc1) || csm.is_locked(rr0,cc1) || csm.is_locked(rr1,cc0))) ||
                    (off_rect  && (csm.is_locked(rr0,cc1) || csm.is_locked(rr1,cc0) || csm.is_locked(rr0,cc0) || csm.is_locked(rr1,cc1)))) {
                    continue;
                }
                const std::size_t d00 = calc_d(rr0,cc0);
                const std::size_t d11 = calc_d(rr1,cc1);
                const std::size_t d01 = calc_d(rr0,cc1);
                const std::size_t d10 = calc_d(rr1,cc0);
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
                if (!::crsce::decompress::detail::dsm_accept_swap(cost_d_before, cost_d_after, d_sat_lost)) {
                    continue;
                }
                // Commit
                if (main_rect) { csm.put(rr0,cc0,false); csm.put(rr1,cc1,false); csm.put(rr0,cc1,true); csm.put(rr1,cc0,true); }
                else { csm.put(rr0,cc1,false); csm.put(rr1,cc0,false); csm.put(rr0,cc0,true); csm.put(rr1,cc1,true); }
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
                ++snap.dsm_swaps;
                snap.dsm_cost_improve_total += static_cast<std::uint64_t>(cost_d_before - cost_d_after);
                if ((snap.dsm_swaps & 0x1FFULL) == 0ULL) { // every 512 accepts
                    maybe_publish(false);
                }
                if (accepts >= trials_per_def) { break; }
            }
            if (accepts == 0) { break; }
        }

        // Validate DSM satisfaction (best-effort): true if all dcnt[i]==dsm[i]
        bool ok = true; for (std::size_t i=0;i<S && i<dsm.size();++i) { if (dcnt[i] != dsm[i]) { ok=false; break; } }
        // Snapshot: lock DSM if desired will be set by caller; just publish residuals snapshot
        snap.U_diag.assign(st.U_diag.begin(), st.U_diag.end());
        maybe_publish(true);
        return ok;
    }
}
