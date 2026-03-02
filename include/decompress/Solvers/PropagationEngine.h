/**
 * @file PropagationEngine.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Concrete propagation engine implementing forcing rules for cardinality constraints.
 */
#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "decompress/Solvers/IConstraintStore.h"
#include "decompress/Solvers/IPropagationEngine.h"
#include "decompress/Solvers/LineID.h"

namespace crsce::decompress::solvers {
    /**
     * @class PropagationEngine
     * @name PropagationEngine
     * @brief Applies forcing rules iteratively until quiescence or infeasibility.
     *
     * When rho(L) = 0, all unknowns on L are forced to 0.
     * When rho(L) = u(L), all unknowns on L are forced to 1.
     * Propagation cascades: each forced assignment may affect other lines.
     */
    class PropagationEngine final : public IPropagationEngine {
    public:
        /**
         * @name kS
         * @brief Matrix dimension.
         */
        static constexpr std::uint16_t kS = 511;

        /**
         * @name PropagationEngine
         * @brief Construct a propagation engine bound to a constraint store.
         * @param store Reference to the constraint store (must outlive this engine).
         * @throws None
         */
        explicit PropagationEngine(IConstraintStore &store);

        bool propagate(std::span<const LineID> queue) override;
        [[nodiscard]] const std::vector<Assignment> &getForcedAssignments() const override;
        void reset() override;

    private:
        /**
         * @name kPropTotalLines
         * @brief Total number of constraint lines: 6s - 2 = 3064.
         */
        static constexpr std::size_t kPropTotalLines = (6 * kS) - 2;

        /**
         * @name store_
         * @brief Reference to the constraint store.
         */
        IConstraintStore &store_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

        /**
         * @name forced_
         * @brief Assignments forced during the last propagation.
         */
        std::vector<Assignment> forced_;

        /**
         * @name work_
         * @brief Reusable work queue for propagation (avoids per-call heap allocation).
         */
        std::vector<LineID> work_;

        /**
         * @name queued_
         * @brief Reusable dedup bitset for the propagation work queue.
         */
        std::bitset<kPropTotalLines> queued_;

    };
} // namespace crsce::decompress::solvers
