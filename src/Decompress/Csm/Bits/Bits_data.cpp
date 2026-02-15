/**
 * @file Bits_data.cpp
 */
#include "decompress/Csm/detail/Bits.h"

namespace crsce::decompress {
    bool Bits::data() const noexcept { return (b_ & kData) != 0U; }
}

