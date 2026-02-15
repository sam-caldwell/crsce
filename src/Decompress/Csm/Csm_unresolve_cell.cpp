/**
 * @file Csm_unresolve_cell.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name unresolve_cell
     * @brief unresolve a cell (resolved cells are readonly)
     * @param r row
     * @param c column
     * @return void
     */
    void Csm::unresolve_cell(const std::size_t r, const std::size_t c) {
        bounds_check(r, c);
        auto &cell = cells_.at(r).at(c);
        cell.set_resolved(false);
    }
}
