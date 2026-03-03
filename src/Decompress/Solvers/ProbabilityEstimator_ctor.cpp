/**
 * @file ProbabilityEstimator_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ProbabilityEstimator constructor implementation.
 */
#include "decompress/Solvers/ProbabilityEstimator.h"

#include "decompress/Solvers/ConstraintStore.h"

namespace crsce::decompress::solvers {

    /**
     * @name ProbabilityEstimator
     * @brief Construct an estimator bound to a constraint store.
     * @param store Const reference to the constraint store.
     * @throws None
     */
    ProbabilityEstimator::ProbabilityEstimator(const ConstraintStore &store)
        : store_(store) {
    }

} // namespace crsce::decompress::solvers
