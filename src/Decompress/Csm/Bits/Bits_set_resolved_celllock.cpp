/**
 * @file Bits_set_resolved_celllock.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"

namespace crsce::decompress {
    /**
     * @name set_resolved
     * @param state cell lock state (locked/unlocked)
     * @return void
     */
    void Bits::set_resolved(const CellLock state) noexcept {
        set_resolved(state == CellLock::Resolved);
    }
}

