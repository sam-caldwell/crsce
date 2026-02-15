/**
 * @file Csm_legacy_is_locked.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name is_locked
     * @brief returns whether cell at (r,c) is locked
     * @param r row
     * @param c column
     * @return boolean
     */
    bool Csm::is_locked(const std::size_t r, const std::size_t c) const {
        return is_locked_rc(r, c);
    }
}
