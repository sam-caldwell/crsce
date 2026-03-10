/**
 * @file ReasonGraph_getAntecedent.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief ReasonGraph::getAntecedent — read the antecedent record for a cell.
 */
#include "decompress/Solvers/ReasonGraph.h"

#include <cstddef>
#include <cstdint>

#include "decompress/Solvers/CellAntecedent.h"

namespace crsce::decompress::solvers {

    /**
     * @name getAntecedent
     * @brief Read the antecedent record for cell (r, c).
     * @param r Row index.
     * @param c Column index.
     * @return Const reference to the CellAntecedent entry.
     */
    const CellAntecedent &ReasonGraph::getAntecedent(const std::uint16_t r,
                                                      const std::uint16_t c) const {
        return table_[(static_cast<std::size_t>(r) * kS) + c]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }

} // namespace crsce::decompress::solvers
