/**
 * @file RowDecomposedController_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief RowDecomposedController constructor implementation.
 */
#include "decompress/Solvers/RowDecomposedController.h"

#include <cassert>
#include <memory>
#include <utility>

#include "decompress/Solvers/BranchingController.h"
#include "decompress/Solvers/IConstraintStore.h"
#include "decompress/Solvers/IHashVerifier.h"
#include "decompress/Solvers/IPropagationEngine.h"
#ifndef NDEBUG
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/Sha256HashVerifier.h"
#endif

namespace crsce::decompress::solvers {

    /**
     * @name RowDecomposedController
     * @brief Construct a row-decomposed controller from solver components.
     * @param store Constraint store (takes ownership).
     * @param propagator Propagation engine (takes ownership).
     * @param brancher Branching controller (takes ownership).
     * @param hasher Hash verifier (takes ownership).
     * @throws None
     */
    RowDecomposedController::RowDecomposedController(std::unique_ptr<IConstraintStore> store,
                                                      std::unique_ptr<IPropagationEngine> propagator,
                                                      std::unique_ptr<BranchingController> brancher,
                                                      std::unique_ptr<IHashVerifier> hasher)
        : store_(std::move(store)),
          propagator_(std::move(propagator)),
          brancher_(std::move(brancher)),
          hasher_(std::move(hasher)) {
#ifndef NDEBUG
        assert(dynamic_cast<ConstraintStore *>(store_.get()));
        assert(dynamic_cast<Sha256HashVerifier *>(hasher_.get()));
#endif
    }

} // namespace crsce::decompress::solvers
