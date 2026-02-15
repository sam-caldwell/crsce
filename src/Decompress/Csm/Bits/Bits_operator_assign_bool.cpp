/**
 * @file Bits_operator_assign_bool.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"

namespace crsce::decompress {
    /**
     * @name assignment operator
     * @brief assignment operator for Bits class
     * @param v boolean data value
     * @return Bits
     */
    Bits &Bits::operator=(const bool v) noexcept {
        set_data(v);
        return *this;
    }
}

