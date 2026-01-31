/**
 * @file GobpSolver_solve_step.cpp
 * @brief Implementation of GobpSolver::solve_step.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Gobp/GobpSolver.h"
#include <cstddef>
#include <cstdlib>    // getenv
#include <iostream>   // std::cerr
#include <cmath>      // std::fabs
#include <algorithm>  // std::min,std::max

namespace crsce::decompress {
    /**
     * @name GobpSolver::solve_step
     * @brief Perform one iteration of GOBP smoothing and assignments.
     * @return std::size_t Number of newly solved cells.
     */
    std::size_t GobpSolver::solve_step() {
        // Debug flag (cached): enable aggregated diagnostics when CRSCE_GOBP_DEBUG is set.
        static int kDbg = -1; // -1=unset, 0=off, 1=on
        if (kDbg < 0) {
            const char *e = std::getenv("CRSCE_GOBP_DEBUG"); // NOLINT(concurrency-mt-unsafe)
            kDbg = (e && *e) ? 1 : 0;
        }

        std::size_t progress = 0;
        const double hi = assign_confidence_;
        const double lo = 1.0 - assign_confidence_;
        // Iterate over grid row-major; compute belief; smooth with existing data; store; assign if extreme

        // Aggregated diagnostics
        std::size_t unlocked_before = 0;
        std::size_t hi_hits = 0;
        std::size_t lo_hits = 0;
        std::size_t ambiguous = 0; // near 0.5 after smoothing
        double min_bel = 1.0;
        double max_bel = 0.0;
        double min_blend = 1.0;
        double max_blend = 0.0;
        double max_delta = 0.0;
        double sum_delta = 0.0;
        constexpr double kAmbiEps = 1e-2; // |p-0.5| < 0.01 considered ambiguous

        for (std::size_t r = 0; r < S; ++r) {
            for (std::size_t c = 0; c < S; ++c) {
                if (csm_.is_locked(r, c)) {
                    continue; // skip assigned cells
                }
                ++unlocked_before;
                const double belief = belief_for(r, c);
                min_bel = std::min(belief, min_bel);
                max_bel = std::max(belief, max_bel);
                // Exponential smoothing (damping)
                const double prev = csm_.get_data(r, c);
                const double blended = (damping_ * prev) + ((1.0 - damping_) * belief);
                csm_.set_data(r, c, blended);
                min_blend = std::min(blended, min_blend);
                max_blend = std::max(blended, max_blend);
                const double d = std::fabs(blended - prev);
                sum_delta += d;
                max_delta = std::max(d, max_delta);
                if (std::fabs(blended - 0.5) < kAmbiEps) { ++ambiguous; }

                if (blended >= hi) {
                    apply_cell(r, c, true);
                    ++progress;
                    ++hi_hits;
                } else if (blended <= lo) {
                    apply_cell(r, c, false);
                    ++progress;
                    ++lo_hits;
                }
            }
        }

        if (kDbg == 1) {
            // Summarize residuals and unknowns remaining (use U_row sum as unknown cell count)
            std::size_t sum_U_row = 0;
            std::size_t sum_R_row = 0;
            for (const auto u : st_.U_row) { sum_U_row += static_cast<std::size_t>(u); }
            for (const auto r : st_.R_row) { sum_R_row += static_cast<std::size_t>(r); }
            const double avg_delta = (unlocked_before > 0) ? (sum_delta / static_cast<double>(unlocked_before)) : 0.0;
            std::cerr
                << "GOBP step: unlocked=" << unlocked_before
                << ", assign_progress=" << progress
                << ", hi_hits=" << hi_hits
                << ", lo_hits=" << lo_hits
                << ", unknown_cells=" << sum_U_row
                << ", row_ones_remaining=" << sum_R_row
                << ", belief[min=" << min_bel << ", max=" << max_bel << "]"
                << ", blended[min=" << min_blend << ", max=" << max_blend << "]"
                << ", delta[avg=" << avg_delta << ", max=" << max_delta << "]"
                << ", ambiguous@0.5=" << ambiguous
                << ", params[damping=" << damping_ << ", conf=" << assign_confidence_ << "]"
                << '\n';
        }
        return progress;
    }
} // namespace crsce::decompress
