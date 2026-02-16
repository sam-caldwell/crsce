/**
 * @file Csm_resolve.cpp
 * @author Sam Caldwell
 * @brief Set cell resolved=true and acquire MU lock.
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name resolve
     * @brief Mark a cell as resolved and lock it.
     * @param r Row index.
     * @param c Column index.
     * @return void
     */
    void Csm::resolve(const std::size_t r, const std::size_t c) {
        bounds_check(r, c);
        auto &cell = cells_.at(r).at(c);
        cell.set_resolved(true);
    }
}
