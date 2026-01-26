/**
 * @file GobpSolver.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Gobp/GobpSolver.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace crsce::decompress {

    /**

     * @brief Implementation detail.

     */

    GobpSolver::GobpSolver(Csm &csm, ConstraintState &state,
                           const double damping,
                           const double assign_confidence)
        : csm_(csm), st_(state), damping_(clamp01(damping)), assign_confidence_(clamp_conf(assign_confidence)) {}

    double GobpSolver::clamp01(const double v) noexcept {
        if (v < 0.0) {
            return 0.0;
        }
        if (v > 1.0) {
            return 1.0;
        }
        return v;
    }

    double GobpSolver::clamp_conf(const double v) noexcept {
        // Clamp to (0.5, 1]; use a small epsilon to avoid equality with 0.5.
        const double clamped = std::clamp(v, 0.5000001, 1.0);
        return clamped;
    }

    double GobpSolver::odds(const double p) noexcept {
        const double eps = 1e-12;
        const double pp = std::clamp(p, eps, 1.0 - eps);
        return pp / (1.0 - pp);
    }

    double GobpSolver::prob_from_odds(const double o) noexcept {
        const double eps = 1e-12;
        const double oo = std::max(o, eps);
        return oo / (1.0 + oo);
    }

    double GobpSolver::belief_for(const std::size_t r, const std::size_t c) const {
        const auto d = diag_index(r, c);
        const auto x = xdiag_index(r, c);

        // For each line family, define line belief p_i as R/U (if 0 < U) with saturation at 0 or 1
        const auto RU = [&](std::uint16_t R, std::uint16_t U) -> double {
            if (U == 0U) {
                // No uncertainty remains on the line; return neutral 0.5 to avoid division issues here.
                return 0.5;
            }
            if (R == 0U) {
                return 0.0;
            }
            if (R == U) {
                return 1.0;
            }
            return static_cast<double>(R) / static_cast<double>(U);
        };

        const double pr = RU(st_.R_row.at(r), st_.U_row.at(r));
        const double pc = RU(st_.R_col.at(c), st_.U_col.at(c));
        const double pd = RU(st_.R_diag.at(d), st_.U_diag.at(d));
        const double px = RU(st_.R_xdiag.at(x), st_.U_xdiag.at(x));

        // Combine line beliefs by multiplying odds (naive independence assumption)
        const double o = odds(pr) * odds(pc) * odds(pd) * odds(px);
        return prob_from_odds(o);
    }

    void GobpSolver::apply_cell(const std::size_t r, const std::size_t c, const bool value) {
        const auto d = diag_index(r, c);
        const auto x = xdiag_index(r, c);

        if (csm_.is_locked(r, c)) {
            return; // already assigned
        }
        if (st_.U_row.at(r) == 0 || st_.U_col.at(c) == 0 || st_.U_diag.at(d) == 0 || st_.U_xdiag.at(x) == 0) {
            throw std::runtime_error("GOBP: U already zero on a line while assigning");
        }
        // Update U first
        --st_.U_row.at(r);
        --st_.U_col.at(c);
        --st_.U_diag.at(d);
        --st_.U_xdiag.at(x);
        // Update R if placing a 1
        if (value) {
            if (st_.R_row.at(r) == 0 || st_.R_col.at(c) == 0 || st_.R_diag.at(d) == 0 || st_.R_xdiag.at(x) == 0) {
                throw std::runtime_error("GOBP: R underflow on a line while assigning 1");
            }
            --st_.R_row.at(r);
            --st_.R_col.at(c);
            --st_.R_diag.at(d);
            --st_.R_xdiag.at(x);
        }

        csm_.put(r, c, value);
        csm_.lock(r, c);
    }

    std::size_t GobpSolver::solve_step() {
        std::size_t progress = 0;
        const double hi = assign_confidence_;
        const double lo = 1.0 - assign_confidence_;
        // Iterate over grid row-major; compute belief; smooth with existing data; store; assign if extreme
        for (std::size_t r = 0; r < S; ++r) {
            for (std::size_t c = 0; c < S; ++c) {
                if (csm_.is_locked(r, c)) {
                    continue; // skip assigned cells
                }
                const double belief = belief_for(r, c);
                // Exponential smoothing (damping)
                const double prev = csm_.get_data(r, c);
                const double blended = (damping_ * prev) + ((1.0 - damping_) * belief);
                csm_.set_data(r, c, blended);

                if (blended >= hi) {
                    apply_cell(r, c, true);
                    ++progress;
                } else if (blended <= lo) {
                    apply_cell(r, c, false);
                    ++progress;
                }
            }
        }
        return progress;
    }

    bool GobpSolver::solved() const {
        for (std::size_t i = 0; i < S; ++i) {
            if (st_.U_row.at(i) != 0 || st_.U_col.at(i) != 0 || st_.U_diag.at(i) != 0 || st_.U_xdiag.at(i) != 0) {
                return false;
            }
        }
        return true;
    }

    void GobpSolver::reset() {
        // No internal state beyond parameters; leave data layer untouched per interface contract
    }

} // namespace crsce::decompress
