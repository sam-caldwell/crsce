/**
 * @file Solver.h
 * @brief Abstract solver interface for CRSCE v1 decompression.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>

namespace crsce::decompress {
    /**
     * @class Solver
     * @name Solver
     * @brief Interface for block-level solvers that operate on the CSM and line constraints.
     *
     * Concrete implementations (e.g., DeterministicElimination, GobpSolver) must implement
     * the step API and report solved status. Implementations may hold references to the
     * CSM and constraint state outside this interface.
     */
    class Solver {
    public:
        /**
         * @name Solver
         * @brief Default construct a Solver base.
         */
        Solver() = default;

        /**
         * @name Solver
         * @brief Deleted copy constructor (non-copyable interface).
         * @param other Unused other instance.
         */
        Solver(const Solver &other) = delete;

        /**
         * @name Solver::operator=
         * @brief Deleted copy assignment (non-copyable interface).
         * @param other Unused other instance.
         * @return Solver& Reference to this.
         */
        Solver &operator=(const Solver &other) = delete;

        /**
         * @name Solver
         * @brief Deleted move constructor (non-movable interface).
         * @param other Unused other instance.
         */
        Solver(Solver &&other) = delete;

        /**
         * @name Solver::operator=
         * @brief Deleted move assignment (non-movable interface).
         * @param other Unused other instance.
         * @return Solver& Reference to this.
         */
        Solver &operator=(Solver &&other) = delete;

        /**
         * @name ~Solver
         * @brief Virtual destructor for interface.
         */
        virtual ~Solver() = default;

        /**
         * @name Solver::solve_step
         * @brief Perform one iteration of solving.
         * @return std::size_t Number of bits newly solved during this step.
         */
        virtual std::size_t solve_step() = 0;

        /**
         * @name Solver::solved
         * @brief Return true if the block is fully solved and consistent.
         * @return bool True if solved; false otherwise.
         */
        [[nodiscard]] virtual bool solved() const = 0;

        /**
         * @name Solver::reset
         * @brief Reset internal state of the solver (does not mutate external CSM/residuals).
         * @return void
         */
        virtual void reset() = 0;
    };
} // namespace crsce::decompress
