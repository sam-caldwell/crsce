/**
 * @file FailedLiteralProber_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief FailedLiteralProber constructor implementation.
 */
#include "decompress/Solvers/FailedLiteralProber.h"

#include "decompress/Solvers/BranchingController.h"
#include "decompress/Solvers/ConstraintStore.h"
#include "decompress/Solvers/IHashVerifier.h"
#include "decompress/Solvers/PropagationEngine.h"

namespace crsce::decompress::solvers {

    /**
     * @name FailedLiteralProber
     * @brief Construct a prober bound to solver components.
     * @param store Reference to the constraint store.
     * @param propagator Reference to the propagation engine.
     * @param brancher Reference to the branching controller.
     * @param hasher Reference to the hash verifier.
     * @throws None
     */
    FailedLiteralProber::FailedLiteralProber(ConstraintStore &store,
                                             PropagationEngine &propagator,
                                             BranchingController &brancher,
                                             IHashVerifier &hasher)
        : store_(store),
          propagator_(propagator),
          brancher_(brancher),
          hasher_(hasher) {
    }

} // namespace crsce::decompress::solvers
