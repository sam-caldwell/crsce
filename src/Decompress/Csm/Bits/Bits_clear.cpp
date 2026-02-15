/**
 * @file Bits_clear.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"

namespace crsce::decompress {
    /**
     * @name clear
     * @brief clear the cell state
     * @return void
     */
    void Bits::clear() noexcept {
        set_raw(0U);
    }
}

