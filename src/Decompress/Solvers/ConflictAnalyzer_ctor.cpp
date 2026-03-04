/**
 * @file ConflictAnalyzer_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief ConflictAnalyzer constructor — allocates the visited-epoch table.
 */
#include "decompress/Solvers/ConflictAnalyzer.h"

#include <cstddef>

#include "decompress/Solvers/ReasonGraph.h"

namespace crsce::decompress::solvers {

    /**
     * @name ConflictAnalyzer
     * @brief Construct a ConflictAnalyzer bound to the given ReasonGraph.
     * @param reasonGraph The reason graph to query.
     * @throws std::bad_alloc if the visited-epoch table cannot be allocated.
     */
    ConflictAnalyzer::ConflictAnalyzer(const ReasonGraph &reasonGraph)
        : reasonGraph_(reasonGraph)
        , visitedEpoch_(static_cast<std::size_t>(kS) * kS, 0)
    {}

} // namespace crsce::decompress::solvers
