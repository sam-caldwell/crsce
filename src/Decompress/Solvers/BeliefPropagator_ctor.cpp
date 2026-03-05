/**
 * @file BeliefPropagator_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief BeliefPropagator constructor — allocate and zero-initialize message tables.
 */
#include "decompress/Solvers/BeliefPropagator.h"

#include <cstddef>

#include "decompress/Solvers/ConstraintStore.h"

namespace crsce::decompress::solvers {

    /**
     * @name BeliefPropagator
     * @brief Allocate msg_ (~8.4 MB) and msgSum_ (~1 MB) on the heap, zero-initialized.
     *        Heap allocation avoids stack overflow in the coroutine frame.
     * @param cs Constraint store reference (not copied; must outlive this object).
     */
    BeliefPropagator::BeliefPropagator(const ConstraintStore &cs)
        : cs_(cs)
        , msg_(static_cast<std::size_t>(kS) * kS * kLineTypes, 0.0F)
        , msgSum_(static_cast<std::size_t>(kS) * kS, 0.0F)
    {}

} // namespace crsce::decompress::solvers
