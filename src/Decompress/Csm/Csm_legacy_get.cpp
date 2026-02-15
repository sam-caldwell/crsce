/**
 * @file Csm_legacy_get.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name get
     * @brief get cell at (r,c)
     * @param r row
     * @param c column
     * @return bool cell state
     */
    bool Csm::get(const std::size_t r, const std::size_t c) const {
        return get_rc(r, c);
    }
}
