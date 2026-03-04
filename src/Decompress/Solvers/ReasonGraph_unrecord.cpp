/**
 * @file ReasonGraph_unrecord.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief ReasonGraph::unrecord — clear the antecedent entry for a cell on undo.
 */
#include "decompress/Solvers/ReasonGraph.h"

#include <cstddef>
#include <cstdint>

#include "decompress/Solvers/CellAntecedent.h"

namespace crsce::decompress::solvers {

    /**
     * @name unrecord
     * @brief Clear the antecedent entry for cell (r, c) on undo.
     * @param r Row index.
     * @param c Column index.
     */
    void ReasonGraph::unrecord(const std::uint16_t r, const std::uint16_t c) {
        table_[(static_cast<std::size_t>(r) * kS) + c] = CellAntecedent{}; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }

} // namespace crsce::decompress::solvers
