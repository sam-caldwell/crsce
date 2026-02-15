/**
 * @file Bits_mu_locked.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"

namespace crsce::decompress {
    /**
     * @name mu_locked
     * @brief returns whether the cell is locked for async operations
     * @return boolean
     */
    bool Bits::mu_locked() const noexcept { return (b_ & kMu) != 0U; }
}

