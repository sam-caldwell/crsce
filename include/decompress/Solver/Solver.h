/**
 * @file Solver.h
 * @brief Abstract solver interface for CRSCE v1 decompression.
 */
#pragma once

#include <cstddef>

namespace crsce::decompress {

    /**
     * @class Solver
     * @brief Interface for block-level solvers that operate on the CSM and line constraints.
     *
     * Concrete implementations (e.g., DeterministicElimination, GobpSolver) must implement
     * the step API and report solved status. Implementations may hold references to the
     * CSM and constraint state outside this interface.
     */
    class Solver {
    public:
        Solver() = default;
        Solver(const Solver&) = delete;
        Solver& operator=(const Solver&) = delete;
        Solver(Solver&&) = delete;
        Solver& operator=(Solver&&) = delete;
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
