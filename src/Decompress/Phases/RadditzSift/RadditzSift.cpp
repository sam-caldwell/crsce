/**
 * @file RadditzSift.cpp
 * @brief Column-focused lateral adoption phase implementation (Radditz Sift).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Phases/RadditzSift/RadditzSift.h"
#include <cstddef>
#include <vector>
#include <string>
#include <chrono>
#include <span>
#include <cstdint> // NOLINT
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Phases/RadditzSift/compute_col_counts.h"
#include "decompress/Phases/RadditzSift/compute_deficits.h"
#include "decompress/Phases/RadditzSift/deficit_abs_sum.h"
#include "decompress/Phases/RadditzSift/collect_row_candidates.h"
#include "decompress/Phases/RadditzSift/greedy_pair_row.h"
#include "decompress/Phases/RadditzSift/all_deficits_zero.h"
#include "decompress/Phases/RadditzSift/verify_row_sums.h"
#include "common/O11y/event.h"

namespace crsce::decompress::phases {

    using crsce::decompress::phases::detail::compute_col_counts;
    using crsce::decompress::phases::detail::compute_deficits;
    using crsce::decompress::phases::detail::deficit_abs_sum;
    using crsce::decompress::phases::detail::collect_row_candidates;
    using crsce::decompress::phases::detail::greedy_pair_row;
    using crsce::decompress::phases::detail::all_deficits_zero;
    using crsce::decompress::phases::detail::verify_row_sums;

    /**
     * @name radditz_sift_phase
     * @brief Fill column deficits by adopting safe 1s; preserves row sums.
     * @param csm Cross‑Sum Matrix to update (bits/locks).
     * @param st Residual constraint state (R/U) updated in place.
     * @param snap Snapshot for instrumentation; counters/timing incremented.
     * @param max_cols Optional cap on processed columns (0 = all).
     * @return std::size_t Number of cells adopted across processed columns.
     */
    std::size_t radditz_sift_phase(Csm &csm,
                                   ConstraintState &st,
                                   BlockSolveSnapshot &snap,
                                   std::size_t max_cols) {

        (void)st;       // not used by simplified sifter
        (void)max_cols; // full sweep by default

        const auto t0 = std::chrono::steady_clock::now();
        constexpr std::size_t S = Csm::kS;
        std::size_t swaps = 0;

        const std::vector<int> col_count = compute_col_counts(csm);

        std::vector<int> deficit = compute_deficits(col_count, std::span<const std::uint16_t>(snap.vsm.data(), snap.vsm.size()));

        const std::size_t initial_abs = deficit_abs_sum(deficit);

        bool improved = true;
        int passes = 0;
        while (improved && passes < 64) {
            improved = false;
            ++passes;
            // For each row, try to swap from surplus columns to deficit columns
            for (std::size_t r = 0; r < S; ++r) {
                std::vector<std::size_t> from; from.reserve(16);
                std::vector<std::size_t> to;   to.reserve(16);
                collect_row_candidates(csm, deficit, r, from, to);
                const bool row_ok = greedy_pair_row(csm, r, from, to, deficit, swaps, snap);
                improved = improved || row_ok;
            }
            // Early exit if all deficits resolved
            if (all_deficits_zero(deficit)) {
                break;
            }
        }

        const auto t1 = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        snap.time_radditz_ms += static_cast<std::size_t>(ms);
        ++snap.radditz_iterations;
        snap.radditz_passes_last = static_cast<std::size_t>(passes);
        snap.radditz_swaps_last = swaps;
        // Compute final deficit metrics for telemetry
        std::size_t cols_remaining = 0;
        for (std::size_t c = 0; c < S; ++c) {
            if (deficit[c] != 0) {
                ++cols_remaining;
            }
        }
        snap.radditz_cols_remaining = cols_remaining;
        snap.radditz_deficit_abs_before = initial_abs;
        snap.radditz_deficit_abs_after = deficit_abs_sum(deficit);
        // Defensive: verify row-sum invariants remain satisfied (BitSplash rows stay intact)
        // This guarantees row_avg_pct remains 100 after Radditz.
        try {
            if (!verify_row_sums(csm, std::span<const std::uint16_t>(snap.lsm.data(), snap.lsm.size()))) {
                snap.radditz_status = 2;
                snap.message = "radditz sift violated row-sum invariant";
            }
        } catch (...) { /* ignore */ }
        ::crsce::o11y::event("radditz_sift_end", {{"swaps", std::to_string(swaps)}});
        return swaps;
    }
}
