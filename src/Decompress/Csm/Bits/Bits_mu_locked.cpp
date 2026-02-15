/**
 * @file Bits_mu_locked.cpp
 */
#include "decompress/Csm/detail/Bits.h"

namespace crsce::decompress {
    bool Bits::mu_locked() const noexcept { return (b_ & kMu) != 0U; }
}

