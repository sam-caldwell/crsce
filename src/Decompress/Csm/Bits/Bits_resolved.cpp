/**
 * @file Bits_resolved.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"

namespace crsce::decompress {
    /**
     * @name resolved
     * @brief return the cell's resolved state
     * @return bool
     */
    bool Bits::resolved() const noexcept {
        return (b_ & kResolved) != 0U;
    }
}
