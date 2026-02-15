/**
 * @file Bits_ctor_raw.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name Bits
     * @brief class constructor
     * @param raw a raw value for storing data, mutex and resolved flags using bitwise operations
     */
    Bits::Bits(const std::uint8_t raw)
        : b_(static_cast<std::uint8_t>(raw & (kData | kMu | kResolved))) {}
}
