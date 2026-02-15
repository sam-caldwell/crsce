/**
 * @file Bits_operator_equals.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"

namespace crsce::decompress {
    /**
     * @name equivalence operator
     * @brief returns true if two bits are equal (full bitwise comparison)
     * @param rhs Bits object
     * @return true/false
     */
    bool Bits::operator==(const Bits &rhs) const noexcept { return b_ == rhs.b_; }
}

