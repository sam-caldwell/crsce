/**
 * @file EnumerationController.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Concrete enumeration controller implementing DFS solution enumeration in lex order.
 */
#pragma once

#include <cstdint>
#include <memory>

#include "common/Csm/Csm.h"
#include "decompress/Solvers/AsyncHashPipeline.h"
#include "decompress/Solvers/BranchingController.h"
#include "decompress/Solvers/IConstraintStore.h"
#include "decompress/Solvers/IEnumerationController.h"
#include "decompress/Solvers/IHashVerifier.h"
#include "decompress/Solvers/IPropagationEngine.h"

namespace crsce::decompress::solvers {
    /**
     * @class EnumerationController
     * @name EnumerationController
     * @brief Coordinates DFS traversal to enumerate feasible CSM solutions in lex order.
     *
     * Implements Algorithm 1 (EnumerateSolutionsLex) from the specification.
     * Composes IConstraintStore, IPropagationEngine, IBranchingController, and IHashVerifier.
     */
    class EnumerationController final : public IEnumerationController {
    public:
        /**
         * @name kS
         * @brief Matrix dimension.
         */
        static constexpr std::uint16_t kS = 511;

        /**
         * @name EnumerationController
         * @brief Construct an enumeration controller from solver components.
         * @param store Constraint store (takes ownership via unique_ptr).
         * @param propagator Propagation engine (takes ownership).
         * @param brancher Branching controller (takes ownership).
         * @param hasher Hash verifier (takes ownership).
         * @throws None
         */
        EnumerationController(std::unique_ptr<IConstraintStore> store,
                              std::unique_ptr<IPropagationEngine> propagator,
                              std::unique_ptr<BranchingController> brancher,
                              std::unique_ptr<IHashVerifier> hasher);

        void enumerate(const SolutionCallback &callback) override;
        auto enumerateSolutionsLex() -> crsce::common::Generator<crsce::common::Csm> override;
        void reset() override;

    private:
        /**
         * @name dfs
         * @brief Iterative DFS traversal for solution enumeration using an explicit stack.
         * @param callback Called for each solution found.
         * @param stop Set to true when callback returns false.
         * @throws None
         */
        void dfs(const SolutionCallback &callback, bool &stop);

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
         * @brief The branching controller.
         */
        std::unique_ptr<BranchingController> brancher_;

        /**
         * @name hasher_
         * @brief The hash verifier.
         */
        std::unique_ptr<IHashVerifier> hasher_;

        /**
         * @name pipeline_
         * @brief Async hash verification pipeline (created per enumeration call).
         */
        std::unique_ptr<AsyncHashPipeline> pipeline_;
    };
} // namespace crsce::decompress::solvers
