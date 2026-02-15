/**
 * @file Bits_resolved.cpp
 */
#include "decompress/Csm/detail/Bits.h"

namespace crsce::decompress {
    bool Bits::resolved() const noexcept { return locked(); }
}

