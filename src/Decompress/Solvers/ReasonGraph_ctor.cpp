/**
 * @file ReasonGraph_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief ReasonGraph constructor — allocates and zero-initialises the antecedent table.
 */
#include "decompress/Solvers/ReasonGraph.h"

#include <cstddef>

namespace crsce::decompress::solvers {

    /**
     * @name ReasonGraph
     * @brief Allocate and zero-initialise the 511×511 antecedent table.
     */
    ReasonGraph::ReasonGraph()
        : table_(static_cast<std::size_t>(kS) * kS)
    {}

} // namespace crsce::decompress::solvers
