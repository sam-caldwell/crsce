/**
 * @file ReasonGraph_recordDecision.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief ReasonGraph::recordDecision — mark a cell as a DFS branching decision.
 */
#include "decompress/Solvers/ReasonGraph.h"

#include <cstddef>
#include <cstdint>

#include "decompress/Solvers/CellAntecedent.h"

namespace crsce::decompress::solvers {

    /**
     * @name recordDecision
     * @brief Mark cell (r, c) as a DFS branching decision at the given stack depth.
     * @param r          Row index.
     * @param c          Column index.
     * @param stackDepth DFS frame index when assigned.
     */
    void ReasonGraph::recordDecision(const std::uint16_t r, const std::uint16_t c,
                                     const std::uint32_t stackDepth) {
        auto &entry          = table_[(static_cast<std::size_t>(r) * kS) + c]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        entry.stackDepth     = stackDepth;
        entry.antecedentLine = CellAntecedent::kNoAntecedent;
        entry.isDecision     = true;
        entry.isAssigned     = true;
    }

} // namespace crsce::decompress::solvers
