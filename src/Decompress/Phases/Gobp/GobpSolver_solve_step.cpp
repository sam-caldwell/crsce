/**
 * @file GobpSolver_solve_step.cpp
 * @brief Implementation of GobpSolver::solve_step.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "decompress/Phases/Gobp/GobpSolver.h"
#include <cstddef>
#include <cstdlib>    // getenv
#include "common/O11y/gobp_debug_enabled.h"
#include "common/O11y/debug_event_gobp.h"
#include <cmath>      // std::fabs
#include <algorithm>  // std::min,std::max
#include <string>

// no local helpers

namespace crsce::decompress {
    /**
     * @name GobpSolver::solve_step
     * @brief Perform one iteration of GOBP smoothing and assignments.
     * @return std::size_t Number of newly solved cells.
     */
    std::size_t GobpSolver::solve_step() {
        // Debug gating is centralized in o11y (CRSCE_GOBP_DEBUG)

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
        // no TTL cooldown in this version

        if (!scan_flipped_) {
            // Default traversal: row-major, top-to-bottom, left-to-right
            for (std::size_t r = 0; r < S; ++r) {
                for (std::size_t c = 0; c < S; ++c) {
                    if (csm_.is_locked(r, c)) { continue; }
                    ++unlocked_before;
                    const std::size_t d = diag_index(r, c);
                    const std::size_t x = xdiag_index(r, c);
                    const double belief = belief_for(r, c);
                    min_bel = std::min(belief, min_bel);
                    max_bel = std::max(belief, max_bel);
                    const double prev = csm_.get_data(r, c);
                    const double blended = (damping_ * prev) + ((1.0 - damping_) * belief);
                    csm_.set_data(r, c, blended);
                    min_blend = std::min(blended, min_blend);
                    max_blend = std::max(blended, max_blend);
                    const double delta = std::fabs(blended - prev);
                    sum_delta += delta;
                    max_delta = std::max(delta, max_delta);
                    if (std::fabs(blended - 0.5) < kAmbiEps) { ++ambiguous; }

                    // Feasibility guards: avoid proposing assignments that are known-impossible
                    const bool line_forbids_one = (st_.R_row.at(r) == 0
                                                || st_.R_col.at(c) == 0
                                                || st_.R_diag.at(d) == 0
                                                || st_.R_xdiag.at(x) == 0);
                    const bool line_forbids_zero = (st_.R_row.at(r) == st_.U_row.at(r)
                                                 || st_.R_col.at(c) == st_.U_col.at(c)
                                                 || st_.R_diag.at(d) == st_.U_diag.at(d)
                                                 || st_.R_xdiag.at(x) == st_.U_xdiag.at(x));

                    if (blended >= hi && !line_forbids_one) {
                        apply_cell(r, c, true);
                        ++progress; ++hi_hits;
                    } else if (blended <= lo && !line_forbids_zero) {
                        apply_cell(r, c, false);
                        ++progress; ++lo_hits;
                    }
                }
            }
        } else {
            // Flipped perspective: column-major, bottom-up, right-to-left
            for (std::size_t c0 = S; c0-- > 0;) {
                for (std::size_t r0 = S; r0-- > 0;) {
                    const std::size_t r = r0;
                    const std::size_t c = c0;
                    if (csm_.is_locked(r, c)) { continue; }
                    ++unlocked_before;
                    const std::size_t d = diag_index(r, c);
                    const std::size_t x = xdiag_index(r, c);
                    const double belief = belief_for(r, c);
                    min_bel = std::min(belief, min_bel);
                    max_bel = std::max(belief, max_bel);
                    const double prev = csm_.get_data(r, c);
                    const double blended = (damping_ * prev) + ((1.0 - damping_) * belief);
                    csm_.set_data(r, c, blended);
                    min_blend = std::min(blended, min_blend);
                    max_blend = std::max(blended, max_blend);
                    const double delta = std::fabs(blended - prev);
                    sum_delta += delta;
                    max_delta = std::max(delta, max_delta);
                    if (std::fabs(blended - 0.5) < kAmbiEps) { ++ambiguous; }

                    // Feasibility guards: avoid proposing assignments that are known-impossible
                    const bool line_forbids_one = (st_.R_row.at(r) == 0
                                                || st_.R_col.at(c) == 0
                                                || st_.R_diag.at(d) == 0
                                                || st_.R_xdiag.at(x) == 0);
                    const bool line_forbids_zero = (st_.R_row.at(r) == st_.U_row.at(r)
                                                 || st_.R_col.at(c) == st_.U_col.at(c)
                                                 || st_.R_diag.at(d) == st_.U_diag.at(d)
                                                 || st_.R_xdiag.at(x) == st_.U_xdiag.at(x));

                    if (blended >= hi && !line_forbids_one) {
                        apply_cell(r, c, true);
                        ++progress; ++hi_hits;
                    } else if (blended <= lo && !line_forbids_zero) {
                        apply_cell(r, c, false);
                        ++progress; ++lo_hits;
                    }
                }
            }
        }

        if (::crsce::o11y::gobp_debug_enabled()) {
            // Summarize residuals and unknowns remaining (use U_row sum as unknown cell count)
            std::size_t sum_U_row = 0;
            std::size_t sum_R_row = 0;
            for (const auto u : st_.U_row) { sum_U_row += static_cast<std::size_t>(u); }
            for (const auto r : st_.R_row) { sum_R_row += static_cast<std::size_t>(r); }
            const double avg_delta = (unlocked_before > 0) ? (sum_delta / static_cast<double>(unlocked_before)) : 0.0;
            ::crsce::o11y::debug_event_gobp(
                "gobp_step_diag",
                {{"unlocked", std::to_string(unlocked_before)},
                 {"assign_progress", std::to_string(progress)},
                 {"hi_hits", std::to_string(hi_hits)},
                 {"lo_hits", std::to_string(lo_hits)},
                 {"unknown_cells", std::to_string(sum_U_row)},
                 {"row_ones_remaining", std::to_string(sum_R_row)},
                 {"belief_min", std::to_string(min_bel)},
                 {"belief_max", std::to_string(max_bel)},
                 {"blended_min", std::to_string(min_blend)},
                 {"blended_max", std::to_string(max_blend)},
                 {"delta_avg", std::to_string(avg_delta)},
                 {"delta_max", std::to_string(max_delta)},
                 {"ambiguous_at_0_5", std::to_string(ambiguous)},
                 {"damping", std::to_string(damping_)},
                 {"conf", std::to_string(assign_confidence_)}});
        }
        return progress;
    }
} // namespace crsce::decompress
