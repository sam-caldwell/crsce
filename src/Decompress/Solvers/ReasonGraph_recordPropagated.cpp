/**
 * @file ReasonGraph_recordPropagated.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief ReasonGraph::recordPropagated — mark a cell as forced by a constraint line.
 */
#include "decompress/Solvers/ReasonGraph.h"

#include <cstddef>
#include <cstdint>

namespace crsce::decompress::solvers {

    /**
     * @name recordPropagated
     * @brief Mark cell (r, c) as forced by a constraint line at the given stack depth.
     * @param r           Row index.
     * @param c           Column index.
     * @param stackDepth  DFS frame index active when propagation was triggered.
     * @param flatLineIdx Flat ConstraintStore line index of the forcing line.
     */
    void ReasonGraph::recordPropagated(const std::uint16_t r, const std::uint16_t c,
                                       const std::uint32_t stackDepth,
                                       const std::uint32_t flatLineIdx) {
        auto &entry          = table_[(static_cast<std::size_t>(r) * kS) + c]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        entry.stackDepth     = stackDepth;
        entry.antecedentLine = flatLineIdx;
        entry.isDecision     = false;
        entry.isAssigned     = true;
    }

} // namespace crsce::decompress::solvers
