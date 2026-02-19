/**
 * @file GenericSolver.h
 * @brief Concrete base class for polymorphic solvers with common state.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>

#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"

namespace crsce::decompress::solvers {
    /**
     * @class GenericSolver
     * @brief A convenience base that stores references to CSM and ConstraintState
     *        and provides default implementations for reset() and solved().
     */
    class GenericSolver {
    public:
        GenericSolver(Csm &csm, ConstraintState &st) noexcept : csm_(csm), st_(st) {}
        virtual ~GenericSolver() = default;

        GenericSolver(const GenericSolver&) = delete;
        GenericSolver &operator=(const GenericSolver&) = delete;
        GenericSolver(GenericSolver&&) = delete;
        GenericSolver &operator=(GenericSolver&&) = delete;

        // Default no-op; concrete solvers may override
        virtual void reset() {}

        // Default solved(): unknowns across families all zero
        [[nodiscard]] virtual bool solved() const {
            return (sum_u(st_.U_row) == 0ULL)
                && (sum_u(st_.U_col) == 0ULL)
                && (sum_u(st_.U_diag) == 0ULL)
                && (sum_u(st_.U_xdiag) == 0ULL);
        }

        // One step of solver progress (pure virtual)
        virtual std::size_t solve_step() = 0;

        // Default solve(): iterate solve_step() until stall or solved
        virtual void solve(std::size_t max_iters = 20000) {
            for (std::size_t it = 0; it < max_iters; ++it) {
                const std::size_t prog = solve_step();
                if (prog == 0 || solved()) { break; }
            }
        }

    protected:
        [[nodiscard]] static std::uint64_t sum_u(const std::array<std::uint16_t, Csm::kS> &v) noexcept {
            std::uint64_t s = 0ULL; for (auto x : v) { s += x; } return s;
        }

        [[nodiscard]] Csm &csm() noexcept { return csm_; }
        [[nodiscard]] ConstraintState &state() noexcept { return st_; }

    private:
        Csm &csm_;
        ConstraintState &st_;
    };
} // namespace crsce::decompress::solvers
