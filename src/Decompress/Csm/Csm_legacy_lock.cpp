/**
 * @file Csm_legacy_lock.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name lock
     * @brief lock a given cell
     * @param r row
     * @param c column
     */
    void Csm::lock(const std::size_t r, const std::size_t c) {
        resolve(r, c);
    }
}
