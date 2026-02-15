/**
 * @file Bits_operators.cpp
 */
#include "decompress/Csm/detail/Bits.h"

namespace crsce::decompress {
    Bits::operator bool() const noexcept { return data(); }

    Bits &Bits::operator=(const bool v) noexcept {
        set_data(v);
        return *this;
    }

    bool Bits::operator==(const Bits &rhs) const noexcept { return b_ == rhs.b_; }
}

