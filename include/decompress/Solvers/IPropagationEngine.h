/**
 * @file IPropagationEngine.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Interface for the constraint propagation engine.
 */
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "decompress/Solvers/LineID.h"

namespace crsce::decompress::solvers {
    /**
     * @struct Assignment
     * @name Assignment
     * @brief A record of a single cell assignment (r, c, value).
     */
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    struct Assignment {
        /**
         * @name r
         * @brief Row index.
         */
        std::uint16_t r;

        /**
         * @name c
         * @brief Column index.
         */
        std::uint16_t c;

        /**
         * @name value
         * @brief Assigned value (0 or 1).
         */
        std::uint8_t value;
    };
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    /**
     * @class IPropagationEngine
     * @name IPropagationEngine
     * @brief Applies forcing rules to a queue of affected lines until quiescence or infeasibility.
     *
     * When rho(L) = 0, forces all unknowns on L to 0.
     * When rho(L) = u(L), forces all unknowns on L to 1.
     * Returns false if any line becomes infeasible (rho < 0 or rho > u).
     */
    class IPropagationEngine {
    public:
        IPropagationEngine(const IPropagationEngine &) = delete;
        IPropagationEngine &operator=(const IPropagationEngine &) = delete;
        IPropagationEngine(IPropagationEngine &&) = delete;
        IPropagationEngine &operator=(IPropagationEngine &&) = delete;

        /**
         * @name ~IPropagationEngine
         * @brief Virtual destructor.
         */
        virtual ~IPropagationEngine() = default;

        /**
         * @name propagate
         * @brief Propagate constraints from a queue of affected lines.
         * @param queue Lines whose statistics may have changed.
         * @return True if all constraints remain feasible; false if a contradiction was found.
         * @throws None
         */
        virtual bool propagate(std::span<const LineID> queue) = 0;

        /**
         * @name getForcedAssignments
         * @brief Retrieve the list of assignments forced during the last propagation.
         * @return Vector of forced assignments.
         * @throws None
         */
        [[nodiscard]] virtual const std::vector<Assignment> &getForcedAssignments() const = 0;

        /**
         * @name reset
         * @brief Clear the list of forced assignments.
         * @throws None
         */
        virtual void reset() = 0;

    protected:
        IPropagationEngine() = default;
    };
} // namespace crsce::decompress::solvers
