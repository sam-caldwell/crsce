/**
 * @file MetalPropagationEngine_getForcedAssignments.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief MetalPropagationEngine::getForcedAssignments implementation.
 */
#include "decompress/Solvers/MetalPropagationEngine.h"

#include <vector>

#include "decompress/Solvers/IPropagationEngine.h"

namespace crsce::decompress::solvers {
    /**
     * @name getForcedAssignments
     * @brief Retrieve the list of assignments forced during the last propagation.
     * @return Const reference to the vector of forced assignments.
     */
    auto MetalPropagationEngine::getForcedAssignments() const -> const std::vector<Assignment> & {
        return forced_;
    }
} // namespace crsce::decompress::solvers
