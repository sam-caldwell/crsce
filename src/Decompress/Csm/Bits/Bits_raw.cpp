/**
 * @file Bits_raw.cpp
 */
#include "decompress/Csm/detail/Bits.h"
#include <cstdint>

namespace crsce::decompress {
    std::uint8_t Bits::raw() const noexcept { return b_; }
}

