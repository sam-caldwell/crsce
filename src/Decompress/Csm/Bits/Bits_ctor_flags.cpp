/**
 * @file Bits_ctor_flags.cpp
 */
#include "decompress/Csm/detail/Bits.h"
#include <cstdint>

namespace crsce::decompress {
    Bits::Bits(const bool data, const bool lock, const bool mu)
        : b_(static_cast<std::uint8_t>((data ? kData : 0U) | (mu ? kMu : 0U) | (lock ? kLock : 0U))) {}
}

