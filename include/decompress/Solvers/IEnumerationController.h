/**
 * @file IEnumerationController.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Interface for the DFS enumeration controller that yields solutions in canonical order.
 */
#pragma once

#include <cstdint>
#include <functional>

#include "common/Csm/Csm.h"
#include "common/Generator/Generator.h"

namespace crsce::decompress::solvers {
    /**
     * @class IEnumerationController
     * @name IEnumerationController
     * @brief Coordinates DFS traversal yielding feasible CSM solutions in lexicographic order.
     *
     * Composes the constraint store, propagation engine, branching controller, and hash
     * verifier to implement Algorithm 1 (EnumerateSolutionsLex) from the specification.
     */
    class IEnumerationController {
    public:
        /**
         * @name SolutionCallback
         * @brief Callback invoked for each feasible solution. Return true to continue, false to stop.
         */
        using SolutionCallback = std::function<bool(const crsce::common::Csm &)>;

        IEnumerationController(const IEnumerationController &) = delete;
        IEnumerationController &operator=(const IEnumerationController &) = delete;
        IEnumerationController(IEnumerationController &&) = delete;
        IEnumerationController &operator=(IEnumerationController &&) = delete;

        /**
         * @name ~IEnumerationController
         * @brief Virtual destructor.
         */
        virtual ~IEnumerationController() = default;

        /**
         * @name enumerate
         * @brief Enumerate feasible solutions in lexicographic order.
         * @param callback Called for each solution. Return true to continue, false to stop early.
         * @throws None
         */
        virtual void enumerate(const SolutionCallback &callback) = 0;

        /**
         * @name enumerateSolutionsLex
         * @brief Coroutine-based generator that lazily yields feasible CSM solutions in lex order.
         *
         * Each iteration resumes the internal DFS and co_yields the next solution.
         * Callers consume solutions with a range-for loop:
         * @code
         *   for (const auto &csm : enumerator.enumerateSolutionsLex()) { ... }
         * @endcode
         * @return A Generator<Csm> that yields solutions one at a time.
         * @throws None
         */
        virtual auto enumerateSolutionsLex() -> crsce::common::Generator<crsce::common::Csm> = 0;

        /**
         * @name reset
         * @brief Reset the enumerator to its initial state for a new constraint set.
         * @throws None
         */
        virtual void reset() = 0;

    protected:
        IEnumerationController() = default;
    };
} // namespace crsce::decompress::solvers
