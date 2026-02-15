/**
 * @file BlockSolver.cpp
 * @author Sam Caldwell
 * @brief High-level block solver: orchestrates DE -> BitSplash -> Radditz -> GOBP with verification.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */

#include "decompress/Block/detail/solve_block.h"

#include <array>
#include <cstddef>
#include <cstdint> // NOLINT
#include <span>
#include <cstdlib>
#include <string>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Utils/detail/decode9.tcc"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "decompress/Block/detail/run_gobp_fallback.h"
#include "decompress/Block/detail/prelock_padded_tail.h"
#include "decompress/Block/detail/seed_initial_beliefs.h"
#include "decompress/Block/detail/init_block_solve_snapshot.h"
#include "decompress/Block/detail/run_de_phase.h"
#include "decompress/Block/detail/execute_bitsplash_and_validate.h"
#include "decompress/Block/detail/execute_radditz_and_validate.h"
#include "decompress/Block/detail/verify_cross_sums_and_lh.h"
#include "decompress/Block/detail/reseed_residuals_from_csm.h"
#include "decompress/Block/detail/pre_polish_commit_any_verified_rows.h"
#include "common/O11y/event.h"

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
                 const std::span<const std::uint8_t> sums,
                 Csm &csm_out,
                 const std::string &seed,
                 const std::uint64_t valid_bits) {
    (void)seed; // the current implementation uses internal seeds; keep param for API stability

    constexpr std::size_t S = Csm::kS;
    constexpr std::size_t vec_bytes = 575U;

    const auto lsm = decode_9bit_stream<S>(sums.subspan(0 * vec_bytes, vec_bytes));
    const auto vsm = decode_9bit_stream<S>(sums.subspan(1 * vec_bytes, vec_bytes));
    const auto dsm = decode_9bit_stream<S>(sums.subspan(2 * vec_bytes, vec_bytes));
    const auto xsm = decode_9bit_stream<S>(sums.subspan(3 * vec_bytes, vec_bytes));

    ConstraintState st{};
    st.R_row = lsm;
    st.R_col = vsm;
    st.R_diag = dsm;
    st.R_xdiag = xsm;
    st.U_row.fill(S);
    st.U_col.fill(S);
    st.U_diag.fill(S);
    st.U_xdiag.fill(S);

    csm_out.reset();
    ::crsce::o11y::event("block_start_de_phase");

    // Pre-lock padding and seed beliefs
    ::crsce::decompress::detail::prelock_padded_tail(csm_out, st, valid_bits);
    DeterministicElimination det{csm_out, st};
    const std::uint64_t belief_seed = ::crsce::decompress::detail::seed_initial_beliefs(csm_out, st);

    // Snapshot and baseline
    BlockSolveSnapshot snap{};
    ::crsce::decompress::detail::init_block_solve_snapshot(
        snap, S, st,
        std::span<const std::uint16_t>(lsm.begin(), lsm.end()),
        std::span<const std::uint16_t>(vsm.begin(), vsm.end()),
        std::span<const std::uint16_t>(dsm.begin(), dsm.end()),
        std::span<const std::uint16_t>(xsm.begin(), xsm.end()),
        belief_seed);
    set_block_solve_snapshot(snap);
    Csm baseline_csm = csm_out;
    ConstraintState baseline_st = st;

    // DE
    static constexpr int kMaxIters = 60000;
    if (!::crsce::decompress::detail::run_de_phase(det, st, csm_out, snap, lh, kMaxIters)) {
        return false;
    }

    // BitSplash (hard fail on mismatch) and Radditz (non-fatal)
    if (!::crsce::decompress::detail::execute_bitsplash_and_validate(csm_out, st, snap, std::span<const std::uint16_t>(lsm))) {
        return false;
    }
    if (!::crsce::decompress::detail::execute_radditz_and_validate(
            csm_out, st, snap,
            std::span<const std::uint16_t>(lsm),
            std::span<const std::uint16_t>(vsm))) {
        // Enforce VSM (and preserve LSM): fail early if columns not satisfied post‑Radditz
        return false;
    }

    // Removed DSM/XSM sifts and gating: proceed with verified rows, reseed, and GOBP fallback.

    // Lock any LH-verified rows immediately after DSM/XSM enforcement
    {
        // Use current state as baseline for commit helper (no separate backups here)
        Csm base_csm = csm_out;
        ConstraintState base_st = st;
        (void)::crsce::decompress::detail::commit_any_verified_rows(csm_out, st,
                                                                    std::span<const std::uint8_t>(lh),
                                                                    base_csm, base_st, snap, /*rs=*/0);
        set_block_solve_snapshot(snap);
    }

    // If solved already, verify and finish
    if (det.solved()) {
        return ::crsce::decompress::detail::verify_cross_sums_and_lh(
            csm_out, lsm, vsm, dsm, xsm, lh, snap);
    }

    // Sync residuals with current CSM so GOBP respects established row/column invariants and DSM/XSM after sifts
    ::crsce::decompress::detail::reseed_residuals_from_csm(
        csm_out, st,
        std::span<const std::uint16_t>(lsm.begin(), lsm.end()),
        std::span<const std::uint16_t>(vsm.begin(), vsm.end()),
        std::span<const std::uint16_t>(dsm.begin(), dsm.end()),
        std::span<const std::uint16_t>(xsm.begin(), xsm.end())
    );

    // GOBP fallback
    const bool solved = ::crsce::decompress::detail::run_gobp_fallback(
        csm_out, st, det, baseline_csm, baseline_st, lh,
        std::span<const std::uint16_t>(lsm), std::span<const std::uint16_t>(vsm),
        std::span<const std::uint16_t>(dsm), std::span<const std::uint16_t>(xsm),
        snap, valid_bits);
    if (!solved) {
        return false;
    }
    return ::crsce::decompress::detail::verify_cross_sums_and_lh(csm_out, lsm, vsm, dsm, xsm, lh, snap);
}

} // namespace crsce::decompress
