/**
 * @file Csm_lock.cpp
 * @author Sam Caldwell
 * @brief Acquire MU lock for a cell (r,c) if not already locked.
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/Csm.h"
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name lock
     * @brief Lock a CSM cell for async operations (if required).
     * @param r Row index.
     * @param c Column index.
     * @return void
     */
    void Csm::lock(const std::size_t r, const std::size_t c) {
        resolve(r, c);
    }
}
