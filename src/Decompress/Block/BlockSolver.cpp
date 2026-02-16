/**
 * @file BlockSolver.cpp
 * @author Sam Caldwell
 * @brief High-level block solver: orchestrates DE → BitSplash → Radditz → Hybrid Sift with final verification.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */

#include "decompress/Block/detail/solve_block.h"

#include <cstddef>
#include <cstdint> // NOLINT
#include <span>
#include <cstdlib>
#include <string>

#include "decompress/Csm/Csm.h"
#include "decompress/CrossSum/CrossSum.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "decompress/Block/detail/prelock_padded_tail.h"
#include "decompress/Block/detail/verify_cross_sums_and_lh.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#ifndef CRSCE_USE_GOBP_ONLY
#include "decompress/Phases/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Block/detail/run_hybrid_sift.h"
#include "decompress/Block/detail/seed_initial_beliefs.h"
#include "decompress/Block/detail/execute_bitsplash_and_validate.h"
#include "decompress/Block/detail/execute_radditz_and_validate.h"
#include "decompress/Block/detail/reseed_residuals_from_csm.h"
#endif
#ifdef CRSCE_USE_GOBP_ONLY
#include "decompress/Solvers/GobpSolver/GobpSolver.h"
// Bring in legacy pipeline headers for fallback if Gobp cannot complete.
#include "decompress/Phases/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Block/detail/run_hybrid_sift.h"
#include "decompress/Block/detail/seed_initial_beliefs.h"
#include "decompress/Block/detail/execute_bitsplash_and_validate.h"
#include "decompress/Block/detail/execute_radditz_and_validate.h"
#include "decompress/Block/detail/reseed_residuals_from_csm.h"
#endif
#include "common/O11y/O11y.h"

namespace crsce::decompress {
    /**
     * @name solve_block
     * @brief Reconstruct a block CSM from LH and cross-sum payloads using the pipeline
     *        DE → BitSplash → Radditz → GOBP, with final cross-sum and LH verification.
     * @param lh Span over the 511 chained LH digests (per-row hash payload).
     * @param sums Span over the serialized cross-sum vectors (LSM, VSM, DSM, XSM) packed as 9-bit values.
     * @param csm_out Output Cross-Sum Matrix to populate on success.
     * @param seed Deterministic seed string (kept for API stability; not used directly).
     * @param valid_bits Number of meaningful bits for this block; bits beyond are treated as zero-locked padding.
     * @return bool True on success (verification passed); false on failure.
     */
    bool solve_block(const std::span<const std::uint8_t> lh,
                     const CrossSums &sums,
                     Csm &csm_out,
                     const std::uint64_t valid_bits) {
        constexpr std::size_t S = Csm::kS;
        static constexpr int kMaxDeIters = 60000;

        const auto &lsm = sums.lsm().targets();
        const auto &vsm = sums.vsm().targets();
        const auto &dsm = sums.dsm().targets();
        const auto &xsm = sums.xsm().targets();
        //
        // Snapshot and constraints
        //
        ConstraintState st(S, lsm, vsm, dsm, xsm);
        BlockSolveSnapshot snap{S,
                                st,
                                std::span<const std::uint16_t>(lsm.begin(), lsm.end()),
                                std::span<const std::uint16_t>(vsm.begin(), vsm.end()),
                                std::span<const std::uint16_t>(dsm.begin(), dsm.end()),
                                std::span<const std::uint16_t>(xsm.begin(), xsm.end()),
                                0ULL};
        set_block_solve_snapshot(snap);
#ifdef CRSCE_USE_GOBP_ONLY
        ::crsce::o11y::O11y::instance().event("gobp_driver_begin");
        // Pre-lock any padded tail cells.
        ::crsce::decompress::detail::prelock_padded_tail(csm_out, st, valid_bits);
        // Run GOBP smoothing/assignment until no progress.
        ::crsce::decompress::solvers::gobp::GobpSolver gobp(csm_out, st, 0.5, 0.995, false);
        for (int it = 0; it < 20000; ++it) {
            const std::size_t prog = gobp.solve_step();
            if (prog == 0 || gobp.solved()) { break; }
        }
        if (::crsce::decompress::detail::verify_cross_sums_and_lh(csm_out, sums, lh, snap)) {
            return true;
        }
        // Fallback: run legacy deterministic + splash + radditz + sift pipeline.
        ::crsce::o11y::O11y::instance().event("gobp_driver_fallback");
        {
            ::crsce::o11y::O11y::instance().event("block_start_de_phase_fallback");
            DeterministicElimination det(kMaxDeIters, csm_out, st, snap, lh);
            if (!det.run()) {
                return false;
            }
            if (det.solved()) {
                return ::crsce::decompress::detail::verify_cross_sums_and_lh(csm_out, sums, lh, snap);
            }
        }
        // Seed beliefs and proceed with BitSplash and Radditz
        const std::uint64_t belief_seed_fb = ::crsce::decompress::detail::seed_initial_beliefs(csm_out, st);
        snap.rng_seed_belief = belief_seed_fb;
        if (!::crsce::decompress::detail::execute_bitsplash_and_validate(
            csm_out, st, snap, std::span<const std::uint16_t>(lsm))) {
            return false;
        }
        if (!::crsce::decompress::detail::execute_radditz_and_validate(
            csm_out, st, snap,
            std::span<const std::uint16_t>(lsm),
            std::span<const std::uint16_t>(vsm))) {
            return false;
        }
        // Sync residuals with current CSM before hybrid sift
        ::crsce::decompress::detail::reseed_residuals_from_csm(
            csm_out, st,
            std::span<const std::uint16_t>(lsm.begin(), lsm.end()),
            std::span<const std::uint16_t>(vsm.begin(), vsm.end()),
            std::span<const std::uint16_t>(dsm.begin(), dsm.end()),
            std::span<const std::uint16_t>(xsm.begin(), xsm.end())
        );

        ::crsce::o11y::O11y::instance().event("hybrid_sift_fallback");
        (void) ::crsce::decompress::detail::run_hybrid_sift(
            csm_out, st, snap,
            std::span<const std::uint8_t>(lh),
            std::span<const std::uint16_t>(dsm.begin(), dsm.end()),
            std::span<const std::uint16_t>(xsm.begin(), xsm.end()));
        set_block_solve_snapshot(snap);

        return ::crsce::decompress::detail::verify_cross_sums_and_lh(csm_out, sums, lh, snap);
#else
        // Fresh CSM provided by caller; no baseline or resets needed.
        //
        // Deterministic Elimination
        //
        ::crsce::o11y::O11y::instance().event("block_start_de_phase");
        DeterministicElimination det(kMaxDeIters, csm_out, st, snap, lh);
        if (!det.run()) {
            return false;
        }
        if (det.solved()) {
            return ::crsce::decompress::detail::verify_cross_sums_and_lh(csm_out, sums, lh, snap);
        }
        //
        // Pre-lock padding and seed beliefs
        //
        ::crsce::decompress::detail::prelock_padded_tail(csm_out, st, valid_bits);
        const std::uint64_t belief_seed = ::crsce::decompress::detail::seed_initial_beliefs(csm_out, st);
        snap.rng_seed_belief = belief_seed;

        // BitSplash (hard fail on mismatch) and Radditz (non-fatal)
        if (!::crsce::decompress::detail::execute_bitsplash_and_validate(
            csm_out, st, snap, std::span<const std::uint16_t>(lsm))) {
            return false;
        }
        if (!::crsce::decompress::detail::execute_radditz_and_validate(
            csm_out, st, snap,
            std::span<const std::uint16_t>(lsm),
            std::span<const std::uint16_t>(vsm))) {
            // Enforce VSM (and preserve LSM): fail early if columns not satisfied post‑Radditz
            return false;
        }

        // Removed DSM/XSM sifts and gating: proceed to optional Hybrid Sift.
        // NOTE: Do not lock any cells here; Bitsplash and Radditz phases must remain unlock-only until final verify.


        // Sync residuals with current CSM so subsequent sifts respect established row/column invariants and DSM/XSM
        ::crsce::decompress::detail::reseed_residuals_from_csm(
            csm_out, st,
            std::span<const std::uint16_t>(lsm.begin(), lsm.end()),
            std::span<const std::uint16_t>(vsm.begin(), vsm.end()),
            std::span<const std::uint16_t>(dsm.begin(), dsm.end()),
            std::span<const std::uint16_t>(xsm.begin(), xsm.end())
        );

        ::crsce::o11y::O11y::instance().event("hybrid_sift");
        (void) ::crsce::decompress::detail::run_hybrid_sift(
            csm_out, st, snap,
            std::span<const std::uint8_t>(lh),
            std::span<const std::uint16_t>(dsm.begin(), dsm.end()),
            std::span<const std::uint16_t>(xsm.begin(), xsm.end()));
            set_block_solve_snapshot(snap);

        // GOBP fallback removed; final verification follows Hybrid Sift.
        return ::crsce::decompress::detail::verify_cross_sums_and_lh(
            csm_out,
            sums,
            lh,
            snap);
#endif
    }
} // namespace crsce::decompress
