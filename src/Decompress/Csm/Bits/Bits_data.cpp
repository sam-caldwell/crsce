/**
 * @file Bits_data.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"

namespace crsce::decompress {
    /**
     * @name data
     * @brief return the cell value (data bit)
     * @return boolean (set/clear) value
     */
    bool Bits::data() const noexcept {
        return (b_ & kData) != 0U;
    }
}

