/**
 * @file Solver.h
 * @brief Abstract solver interface for CRSCE v1 decompression.
 * © Sam Caldwell.  See LICENSE.txt for details
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
         * @name operator=
         * @brief Deleted copy assignment (non-copyable interface).
         * @param other Unused other instance.
         * @return N/A
         */
        Solver &operator=(const Solver &other) = delete;

        /**
         * @name Solver
         * @brief Deleted move constructor (non-movable interface).
         * @param other Unused other instance.
         */
        Solver(Solver &&other) = delete;

        /**
         * @name operator=
         * @brief Deleted move assignment (non-movable interface).
         * @param other Unused other instance.
         * @return N/A
         */
        Solver &operator=(Solver &&other) = delete;

        /**
         * @name ~Solver
         * @brief Virtual destructor for interface.
         */
        virtual ~Solver() = default;

        /**
         * @brief Perform one iteration of solving.
         * @return Number of bits newly solved during this step.
         */
        virtual std::size_t solve_step() = 0;

        /**
         * @brief Return true if the block is fully solved and consistent.
         */
        [[nodiscard]] virtual bool solved() const = 0;

        /**
         * @brief Reset internal state of the solver (does not mutate external CSM/residuals).
         */
        virtual void reset() = 0;
    };
} // namespace crsce::decompress
