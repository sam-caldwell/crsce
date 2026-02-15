/**
 * @file Bits_locked.cpp
 */
#include "decompress/Csm/detail/Bits.h"

namespace crsce::decompress {
    bool Bits::locked() const noexcept { return (b_ & kLock) != 0U; }
}

