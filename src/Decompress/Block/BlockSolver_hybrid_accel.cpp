/**
 * @file BlockSolver_hybrid_accel.cpp
 * @brief Accelerated Hybrid Sift wrapper implementation. Uses GPU (Metal) if available
 *        to propose high-quality 2x2 rectangle candidates, then applies/validates on CPU
 *        using the same invariants as the baseline hybrid sift. Falls back to
 *        run_hybrid_sift() when GPU is unavailable or disabled at build time.
 * @author Sam Caldwell
 * © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#include "decompress/Block/detail/run_hybrid_sift_accel.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <cstdlib>
#include <vector>
#include <random>
#include <chrono>

#include "decompress/Block/detail/run_hybrid_sift.h"
#include "decompress/Gpu/Metal/RectScorer.h"
#include "decompress/Gpu/Metal/Types.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/HashMatrix/LateralHashMatrix.h"
#include "decompress/CrossSum/DiagonalSumMatrix.h"
#include "decompress/CrossSum/AntiDiagonalSumMatrix.h"
#include "decompress/CrossSum/VerticalSumMatrix.h"
#include "decompress/Utils/detail/index_calc_d.h"
#include "decompress/Utils/detail/index_calc_x.h"
#include "decompress/Block/detail/hybrid_clamp_need.h"
#include "decompress/Block/detail/get_block_solve_snapshot.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "common/O11y/O11y.h"
#include <utility>
#include <algorithm>
#include <limits>
// For env-driven GPU candidate sample override

namespace crsce::decompress::detail {

    // Forward-declare minimal GPU entry (hidden, optional)
    namespace {
        using crsce::decompress::gpu::metal::RectCandidate;

        // Compute whole-matrix counts once
        void compute_counts(const Csm &csm,
                                   std::vector<std::uint16_t> &dcnt,
                                   std::vector<std::uint16_t> &xcnt,
                                   std::vector<std::uint16_t> &vcnt) {
            constexpr std::size_t S = Csm::kS;
            dcnt.assign(S, 0U); xcnt.assign(S, 0U); vcnt.assign(S, 0U);
            for (std::size_t r = 0; r < S; ++r) {
                for (std::size_t c = 0; c < S; ++c) {
                    if (!csm.get(r, c)) { continue; }
                    const auto d = ::crsce::decompress::detail::calc_d(r, c);
                    const auto x = ::crsce::decompress::detail::calc_x(r, c);
                    ++dcnt[d]; ++xcnt[x]; ++vcnt[c];
                }
            }
        }

        // Build a batch of rectangle candidates that preserve row sums and avoid locks
        std::vector<RectCandidate>
        sample_candidates(const Csm &csm, std::size_t max_samples) {
            constexpr std::size_t S = Csm::kS;
            std::vector<RectCandidate> out; out.reserve(max_samples);
            std::mt19937_64 rng{static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
            std::uniform_int_distribution<std::size_t> dist_row(0, S-1);
            std::uniform_int_distribution<std::size_t> dist_col(0, S-1);
            std::size_t attempts = 0;
            const std::size_t max_attempts = max_samples * 16ULL;
            while (out.size() < max_samples && attempts < max_attempts) {
                ++attempts;
                std::size_t r0 = dist_row(rng);
                std::size_t r1 = dist_row(rng);
                if (r0 == r1) { continue; }
                if (r1 < r0) { std::swap(r0, r1); }

                // Find c0 where row r0 has 1 and row r1 has 0 (both unlocked)
                std::size_t c0 = dist_col(rng);
                bool ok0 = false;
                for (std::size_t iter = 0; iter < S; ++iter) {
                    const std::size_t c = (c0 + iter) % S;
                    if (csm.is_locked(r0, c) || csm.is_locked(r1, c)) { continue; }
                    if (csm.get(r0, c) && !csm.get(r1, c)) { c0 = c; ok0 = true; break; }
                }
                if (!ok0) { continue; }

                // Find c1 where row r0 has 0 and row r1 has 1 (both unlocked), c1 != c0
                std::size_t c1 = dist_col(rng);
                bool ok1 = false;
                for (std::size_t iter = 0; iter < S; ++iter) {
                    const std::size_t c = (c1 + iter) % S;
                    if (c == c0) { continue; }
                    if (csm.is_locked(r0, c) || csm.is_locked(r1, c)) { continue; }
                    if (!csm.get(r0, c) && csm.get(r1, c)) { c1 = c; ok1 = true; break; }
                }
                if (!ok1) { continue; }

                // Main pattern (1,0 ; 0,1) → (0,1 ; 1,0) deltas = [-1, +1, +1, -1]
                const std::size_t d00 = ::crsce::decompress::detail::calc_d(r0, c0);
                const std::size_t d01 = ::crsce::decompress::detail::calc_d(r0, c1);
                const std::size_t d10 = ::crsce::decompress::detail::calc_d(r1, c0);
                const std::size_t d11 = ::crsce::decompress::detail::calc_d(r1, c1);
                const std::size_t x00 = ::crsce::decompress::detail::calc_x(r0, c0);
                const std::size_t x01 = ::crsce::decompress::detail::calc_x(r0, c1);
                const std::size_t x10 = ::crsce::decompress::detail::calc_x(r1, c0);
                const std::size_t x11 = ::crsce::decompress::detail::calc_x(r1, c1);

                RectCandidate rc{};
                rc.d[0] = static_cast<std::uint16_t>(d00);
                rc.d[1] = static_cast<std::uint16_t>(d01);
                rc.d[2] = static_cast<std::uint16_t>(d10);
                rc.d[3] = static_cast<std::uint16_t>(d11);
                rc.x[0] = static_cast<std::uint16_t>(x00);
                rc.x[1] = static_cast<std::uint16_t>(x01);
                rc.x[2] = static_cast<std::uint16_t>(x10);
                rc.x[3] = static_cast<std::uint16_t>(x11);
                rc.delta[0] = static_cast<std::int8_t>(-1);
                rc.delta[1] = static_cast<std::int8_t>(+1);
                rc.delta[2] = static_cast<std::int8_t>(+1);
                rc.delta[3] = static_cast<std::int8_t>(-1);
                out.push_back(rc);
            }
            return out;
        }

        // Returns 0 if GPU path not available. On success, returns number of accepted swaps (0 or 1).
        std::size_t run_hybrid_sift_gpu_try(Csm &csm,
                                            ConstraintState &st,
                                            [[maybe_unused]] BlockSolveSnapshot &snap,
                                            const ::crsce::decompress::hashes::LateralHashMatrix & /*lh*/,
                                            const ::crsce::decompress::xsum::VerticalSumMatrix &vsm_t,
                                            const ::crsce::decompress::xsum::DiagonalSumMatrix &dsm_t,
                                            const ::crsce::decompress::xsum::AntiDiagonalSumMatrix &xsm_t) {
#ifdef __APPLE__
            constexpr std::size_t S = Csm::kS;
            const auto dsm = dsm_t.targets();
            const auto xsm = xsm_t.targets();
            const auto vsm = vsm_t.targets();

            // Compute counts
            std::vector<std::uint16_t> dcnt;
            std::vector<std::uint16_t> xcnt;
            std::vector<std::uint16_t> vcnt;
            compute_counts(csm, dcnt, xcnt, vcnt);

            // Build candidates with large baked-in default; allow env override via CRSCE_HYB_GPU_SAMPLES
            std::size_t samples = 1048576ULL;
            if (const char *p = std::getenv("CRSCE_HYB_GPU_SAMPLES"); p && *p) { // NOLINT(concurrency-mt-unsafe)
                try {
                    const auto v = std::stoull(std::string(p));
                    if (v >= 16384ULL) { samples = v; }
                } catch (...) { /* ignore parse errors */ }
            }
            const auto cands = sample_candidates(csm, samples);
            if (cands.empty()) { return 0U; }

            // Score on GPU (or return false if unavailable); measure elapsed without perturbing GPU path
            std::vector<std::int32_t> scores;
            const auto t0 = std::chrono::steady_clock::now();
            const bool ok = ::crsce::decompress::gpu::metal::score_rectangles(
                dcnt, xcnt, vcnt,
                std::vector<std::uint16_t>(dsm.begin(), dsm.end()),
                std::vector<std::uint16_t>(xsm.begin(), xsm.end()),
                std::vector<std::uint16_t>(vsm.begin(), vsm.end()),
                cands, scores);
            // Note: Above call needs correct VSM targets. Reuse AntiDiagonalSumMatrix type for convenience is not ideal.
            const auto t1 = std::chrono::steady_clock::now();
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            // Emit best GPU delta (minimum score)
            if (!scores.empty()) {
                auto it = std::ranges::min_element(scores);
                if (it != scores.end()) {
                    ::crsce::o11y::O11y::instance().event("hyb_gpu_best_delta",
                        {{"delta", std::to_string(*it)}, {"N", std::to_string(scores.size())}, {"elapsed_ms", std::to_string(ms)}});
                }
            }

            // Publish non-blocking o11y metric for GPU scoring throughput
            ::crsce::o11y::O11y::instance().metric("gpu_rectangles_scored",
                static_cast<std::int64_t>(scores.size()), {{"elapsed_ms", std::to_string(ms)}});

            // Update snapshot attempts (rectangles evaluated)
            if (auto cur = get_block_solve_snapshot(); cur.has_value()) {
                auto s2 = *cur;
                s2.hyb_rect_attempts_total += scores.size();
                set_block_solve_snapshot(s2);
            }

            if (!ok) {
                return 0U;
            }

            // Pick best improving candidate
            std::size_t best_i = std::numeric_limits<std::size_t>::max();
            std::int32_t best_dphi = 0;
            for (std::size_t i = 0; i < scores.size(); ++i) {
                if (scores[i] < 0 && (best_i == std::numeric_limits<std::size_t>::max() || scores[i] < best_dphi)) {
                    best_i = i; best_dphi = scores[i];
                }
            }
            if (best_i == std::numeric_limits<std::size_t>::max()) { return 0U; }

            // Reconstruct (r0,c0,r1,c1) from indices by scanning for matching pair (approximate, as we didn't store rows)
            // Instead, apply by recomputing which cells correspond to candidate indices using the relationship d=(c-r)mod S and x=(r+c)mod S.
            // Solve for (r,c) given (d,x): r = ((x - d) mod S), c = ((x + d) mod S) / 2 under toroidal mapping with S odd.
            auto solve_rc = [](std::size_t d, std::size_t x) -> std::pair<std::size_t,std::size_t> {
                constexpr std::size_t S2 = Csm::kS;
                // For odd S, r = (x - d) mod S, c = (x + d) mod S, then c = (r + d) mod S consistency holds.
                const std::size_t r = (x + S2 - d) % S2;
                const std::size_t c = (r + d) % S2;
                return {r, c};
            };

            const auto &rc = cands.at(best_i);
            const auto [r0c0_r, r0c0_c] = solve_rc(rc.d[0], rc.x[0]);
            const auto [r0c1_r, r0c1_c] = solve_rc(rc.d[1], rc.x[1]);
            const auto [r1c0_r, r1c0_c] = solve_rc(rc.d[2], rc.x[2]);
            const auto [r1c1_r, r1c1_c] = solve_rc(rc.d[3], rc.x[3]);

            // Sanity check: rows should be r0 == r0c1_r and r1 == r1c1_r
            const bool pattern_ok = (r0c0_r == r0c1_r) && (r1c0_r == r1c1_r) &&
                                    (r0c0_r != r1c0_r) && (r0c0_c != r0c1_c) &&
                                    !csm.is_locked(r0c0_r, r0c0_c) && !csm.is_locked(r0c0_r, r0c1_c) &&
                                    !csm.is_locked(r1c0_r, r1c0_c) && !csm.is_locked(r1c0_r, r1c1_c);
            if (!pattern_ok) { return 0U; }

            // Apply swap to CSM, update local counts and residuals as in baseline
            // Deltas reflect main pattern [-1,+1,+1,-1]
            const std::size_t d00 = rc.d[0];
            const std::size_t d01 = rc.d[1];
            const std::size_t d10 = rc.d[2];
            const std::size_t d11 = rc.d[3];
            const std::size_t x00 = rc.x[0];
            const std::size_t x01 = rc.x[1];
            const std::size_t x10 = rc.x[2];
            const std::size_t x11 = rc.x[3];

            csm.clear(r0c0_r, r0c0_c); csm.clear(r1c1_r, r1c1_c);
            csm.set  (r0c1_r, r0c1_c); csm.set  (r1c0_r, r1c0_c);

            auto clamp_need = ::crsce::decompress::detail::clamp_need;

            // Update counts (local arrays) and residuals R for only affected indices
            auto inc16 = [](std::uint16_t &v, int d){ v = static_cast<std::uint16_t>(static_cast<int>(v) + d); };
            inc16(dcnt.at(d00), -1); inc16(dcnt.at(d11), -1); inc16(dcnt.at(d01), +1); inc16(dcnt.at(d10), +1);
            inc16(xcnt.at(x00), -1); inc16(xcnt.at(x11), -1); inc16(xcnt.at(x01), +1); inc16(xcnt.at(x10), +1);

            const auto U_d00 = st.U_diag.at(d00); const auto need_d00 = (dsm[d00] > dcnt.at(d00)) ? static_cast<std::uint16_t>(dsm[d00] - dcnt.at(d00)) : 0U; st.R_diag.at(d00) = clamp_need(need_d00, U_d00);
            const auto U_d11 = st.U_diag.at(d11); const auto need_d11 = (dsm[d11] > dcnt.at(d11)) ? static_cast<std::uint16_t>(dsm[d11] - dcnt.at(d11)) : 0U; st.R_diag.at(d11) = clamp_need(need_d11, U_d11);
            const auto U_d01 = st.U_diag.at(d01); const auto need_d01 = (dsm[d01] > dcnt.at(d01)) ? static_cast<std::uint16_t>(dsm[d01] - dcnt.at(d01)) : 0U; st.R_diag.at(d01) = clamp_need(need_d01, U_d01);
            const auto U_d10 = st.U_diag.at(d10); const auto need_d10 = (dsm[d10] > dcnt.at(d10)) ? static_cast<std::uint16_t>(dsm[d10] - dcnt.at(d10)) : 0U; st.R_diag.at(d10) = clamp_need(need_d10, U_d10);
            const auto U_x00 = st.U_xdiag.at(x00); const auto need_x00 = (xsm[x00] > xcnt.at(x00)) ? static_cast<std::uint16_t>(xsm[x00] - xcnt.at(x00)) : 0U; st.R_xdiag.at(x00) = clamp_need(need_x00, U_x00);
            const auto U_x11 = st.U_xdiag.at(x11); const auto need_x11 = (xsm[x11] > xcnt.at(x11)) ? static_cast<std::uint16_t>(xsm[x11] - xcnt.at(x11)) : 0U; st.R_xdiag.at(x11) = clamp_need(need_x11, U_x11);
            const auto U_x01 = st.U_xdiag.at(x01); const auto need_x01 = (xsm[x01] > xcnt.at(x01)) ? static_cast<std::uint16_t>(xsm[x01] - xcnt.at(x01)) : 0U; st.R_xdiag.at(x01) = clamp_need(need_x01, U_x01);
            const auto U_x10 = st.U_xdiag.at(x10); const auto need_x10 = (xsm[x10] > xcnt.at(x10)) ? static_cast<std::uint16_t>(xsm[x10] - xcnt.at(x10)) : 0U; st.R_xdiag.at(x10) = clamp_need(need_x10, U_x10);

            // Snapshot counters (best-effort): only update accept; attempts were accounted via GPU scoring size
            if (auto cur = get_block_solve_snapshot(); cur.has_value()) {
                auto s2 = *cur;
                s2.hyb_rect_accepts_total += 1;
                set_block_solve_snapshot(s2);
            }
            return 1U;
#else
            (void)csm; (void)st; (void)snap; (void)dsm_t; (void)xsm_t; return 0U;
#endif
        }
    } // namespace

    std::size_t run_hybrid_sift_accel(Csm &csm,
                                      ConstraintState &st,
                                      BlockSolveSnapshot &snap,
                                      const ::crsce::decompress::hashes::LateralHashMatrix &lh,
                                      const ::crsce::decompress::xsum::VerticalSumMatrix &vsm_t,
                                      const ::crsce::decompress::xsum::DiagonalSumMatrix &dsm,
                                      const ::crsce::decompress::xsum::AntiDiagonalSumMatrix &xsm) {
        ::crsce::o11y::O11y::instance().event("hybrid_sift_accel_start");
        std::size_t total_accepts = 0;
        // Try GPU-accelerated seed pass first; always follow with base to continue convergence
        const std::size_t gpu_accepts = run_hybrid_sift_gpu_try(csm, st, snap, lh, vsm_t, dsm, xsm);
        if (gpu_accepts > 0) {
            ::crsce::o11y::O11y::instance().event("hybrid_sift_accel_end", {{"accepted", std::to_string(gpu_accepts)}});
            total_accepts += gpu_accepts;
        }
        // Continue with the original deterministic hybrid sift for additional progress
        const std::size_t base = run_hybrid_sift(csm, st, snap, lh, dsm, xsm);
        ::crsce::o11y::O11y::instance().event("hybrid_sift_accel_fallback", {{"accepted", std::to_string(base)}});
        total_accepts += base;
        return total_accepts;
    }
}
