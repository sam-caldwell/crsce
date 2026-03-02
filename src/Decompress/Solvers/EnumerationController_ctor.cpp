/**
 * @file EnumerationController_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief EnumerationController constructor implementation.
 */
#include "decompress/Solvers/EnumerationController.h"

#include <memory>
#include <utility>

#include "decompress/Solvers/BranchingController.h"
#include "decompress/Solvers/IConstraintStore.h"
#include "decompress/Solvers/IHashVerifier.h"
#include "decompress/Solvers/IPropagationEngine.h"

namespace crsce::decompress::solvers {
    /**
     * @name EnumerationController
     * @brief Construct an enumeration controller from solver components.
     * @param store Constraint store (takes ownership).
     * @param propagator Propagation engine (takes ownership).
     * @param brancher Branching controller (takes ownership).
     * @param hasher Hash verifier (takes ownership).
     * @throws None
     */
    EnumerationController::EnumerationController(std::unique_ptr<IConstraintStore> store,
                                                  std::unique_ptr<IPropagationEngine> propagator,
                                                  std::unique_ptr<BranchingController> brancher,
                                                  std::unique_ptr<IHashVerifier> hasher)
        : store_(std::move(store)),
          propagator_(std::move(propagator)),
          brancher_(std::move(brancher)),
          hasher_(std::move(hasher)) {}
} // namespace crsce::decompress::solvers
