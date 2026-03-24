/**
 * @file RowDecomposedController.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Probability-guided global DFS solver with cross-row hash pruning for CSM reconstruction.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "common/Csm/Csm.h"
#include "decompress/Solvers/BranchingController.h"
#include "decompress/Solvers/IConstraintStore.h"
#include "decompress/Solvers/IEnumerationController.h"
#include "decompress/Solvers/IHashVerifier.h"
#include "decompress/Solvers/IPropagationEngine.h"
#include "decompress/Solvers/ProbabilityEstimator.h"

namespace crsce::decompress::solvers {

    /**
     * @class RowDecomposedController
     * @name RowDecomposedController
     * @brief Probability-guided global DFS solver with cross-row hash pruning.
     *
     * Performs a global cell-by-cell DFS across the full 511x511 matrix, using
     * probability estimates to order cells by confidence (most constrained first)
     * and to choose the preferred branch value first. Cross-row hash verification
     * prunes subtrees as soon as any row becomes fully assigned: if its SHA-256
     * hash mismatches the lateral hash, the entire subtree is abandoned.
     */
    class RowDecomposedController final : public IEnumerationController {
    public:
        /**
         * @name kS
         * @brief Matrix dimension.
         */
        static constexpr std::uint16_t kS = 127;

        /**
         * @name RowDecomposedController
         * @brief Construct a row-decomposed controller from solver components.
         * @param store Constraint store (takes ownership via unique_ptr).
         * @param propagator Propagation engine (takes ownership).
         * @param brancher Branching controller (takes ownership, used for undo stack only).
         * @param hasher Hash verifier (takes ownership).
         * @throws None
         */
        RowDecomposedController(std::unique_ptr<IConstraintStore> store,
                                std::unique_ptr<IPropagationEngine> propagator,
                                std::unique_ptr<BranchingController> brancher,
                                std::unique_ptr<IHashVerifier> hasher);

        void enumerate(const SolutionCallback &callback) override;
        auto enumerateSolutionsLex() -> crsce::common::Generator<crsce::common::Csm> override;
        void reset() override;

    private:
        /**
         * @name buildCsm
         * @brief Build a CSM from the current constraint store state.
         * @return A fully-assigned CSM.
         * @throws None
         */
        [[nodiscard]] crsce::common::Csm buildCsm() const;

        /**
         * @name store_
         * @brief The constraint store.
         */
        std::unique_ptr<IConstraintStore> store_;

        /**
         * @name propagator_
         * @brief The propagation engine.
         */
        std::unique_ptr<IPropagationEngine> propagator_;

        /**
         * @name brancher_
         * @brief The branching controller (used for undo stack operations only).
         */
        std::unique_ptr<BranchingController> brancher_;

        /**
         * @name hasher_
         * @brief The hash verifier.
         */
        std::unique_ptr<IHashVerifier> hasher_;
    };

} // namespace crsce::decompress::solvers
