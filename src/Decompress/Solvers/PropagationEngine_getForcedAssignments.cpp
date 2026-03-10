/**
 * @file PropagationEngine_getForcedAssignments.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief PropagationEngine::getForcedAssignments implementation.
 */
#include "decompress/Solvers/PropagationEngine.h"

#include <vector>

#include "decompress/Solvers/IPropagationEngine.h"

namespace crsce::decompress::solvers {
    /**
     * @name getForcedAssignments
     * @brief Retrieve the list of assignments forced during the last propagation.
     * @return Const reference to the vector of forced assignments.
     * @throws None
     */
    auto PropagationEngine::getForcedAssignments() const -> const std::vector<Assignment> & {
        return forced_;
    }
} // namespace crsce::decompress::solvers
