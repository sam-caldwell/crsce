/**
 * @file Bits_raw.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name raw
     * @brief return the raw cell content
     * @return uint8_t
     */
    std::uint8_t Bits::raw() const noexcept { return b_; }
}

