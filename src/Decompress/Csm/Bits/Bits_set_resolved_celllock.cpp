/**
 * @file Bits_set_resolved_celllock.cpp
 * @author Sam Caldwell
 * @brief Overload to set resolved state using CellLock enum.
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"

namespace crsce::decompress {
    /**
     * @name set_resolved
     * @brief Set resolved flag via CellLock enum value.
     * @param state Cell lock state (Unsolved|Resolved).
     * @return void
     */
    void Bits::set_resolved(const CellLock state) noexcept {
        set_resolved(state == CellLock::Resolved);
    }
}
