/**
 * @file GobpSolver.h
 * @brief CPU-based, single-host Game Of Beliefs Protocol (GOBP) solver.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/ConstraintState.h"
#include "decompress/Solver/Solver.h"

namespace crsce::decompress {
    class GobpSolver : public Solver {
    public:
        GobpSolver(Csm &csm, ConstraintState &state,
                   double damping = 0.5,
                   double assign_confidence = 0.995);
        std::size_t solve_step() override;
        [[nodiscard]] bool solved() const override;
        void reset() override;

        [[nodiscard]] double damping() const noexcept { return damping_; }
        [[nodiscard]] double assign_confidence() const noexcept { return assign_confidence_; }
        void set_damping(double d) noexcept { damping_ = clamp01(d); }
        void set_assign_confidence(double c) noexcept { assign_confidence_ = clamp_conf(c); }
        void set_scan_flipped(bool v) noexcept { scan_flipped_ = v; }

    private:
        Csm &csm_;
        ConstraintState &st_;
        double damping_{0.5};
        double assign_confidence_{0.995};
        static constexpr std::size_t S = Csm::kS;
        bool scan_flipped_{false};

        static constexpr std::size_t diag_index(std::size_t r, std::size_t c) noexcept {
            return (c >= r) ? (c - r) : (c + S - r);
        }
        static constexpr std::size_t xdiag_index(std::size_t r, std::size_t c) noexcept {
            return (r + c) % S;
        }

        static double clamp01(double v) noexcept;
        static double clamp_conf(double v) noexcept;
        static double odds(double p) noexcept;
        static double prob_from_odds(double o) noexcept;
        [[nodiscard]] double belief_for(std::size_t r, std::size_t c) const;
        static auto calculate_belief(std::size_t R, std::size_t U) -> double;
        void apply_cell(std::size_t r, std::size_t c, bool value);
    };
} // namespace crsce::decompress

