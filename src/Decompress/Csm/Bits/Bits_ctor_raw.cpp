/**
 * @file Bits_ctor_raw.cpp
 */
#include "decompress/Csm/detail/Bits.h"
#include <cstdint>

namespace crsce::decompress {
    Bits::Bits(const std::uint8_t raw)
        : b_(static_cast<std::uint8_t>(raw & (kData | kMu | kLock))) {}
}

