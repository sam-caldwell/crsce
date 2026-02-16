/**
 * @file GobpSolver.h
 * @brief Adapter Gobp solver derived from GenericSolver for A/B switching.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "decompress/Solvers/GenericSolver.h"
#include "decompress/Phases/Gobp/GobpSolver.h" // underlying engine

namespace crsce::decompress::solvers::gobp {
    /**
     * @class GobpSolver
     * @brief Thin wrapper around the current Gobp engine implementing GenericSolver.
     */
    class GobpSolver : public ::crsce::decompress::solvers::GenericSolver {
    public:
        GobpSolver(Csm &csm,
                   ConstraintState &st,
                   double damping = 0.5,
                   double assign_confidence = 0.995,
                   bool scan_flipped = false)
            : GenericSolver(csm, st), engine_(csm, st, damping, assign_confidence) {
            engine_.set_scan_flipped(scan_flipped);
        }

        std::size_t solve_step() override { return engine_.solve_step(); }
        void reset() override { engine_.reset(); }
        // solved(): use GenericSolver's default (unknown totals) or delegate to engine_.solved().
        [[nodiscard]] bool solved() const override { return GenericSolver::solved(); }

        // Parameter helpers for A/B config
        void set_damping(double d) noexcept { engine_.set_damping(d); }
        void set_assign_confidence(double c) noexcept { engine_.set_assign_confidence(c); }
        void set_scan_flipped(bool v) noexcept { engine_.set_scan_flipped(v); }
        [[nodiscard]] double damping() const noexcept { return engine_.damping(); }
        [[nodiscard]] double assign_confidence() const noexcept { return engine_.assign_confidence(); }

    private:
        ::crsce::decompress::GobpSolver engine_;
    };
} // namespace crsce::decompress::solvers::gobp

