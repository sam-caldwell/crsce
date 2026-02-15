/**
 * @file Bits_operator_bool.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/Csm/detail/Bits.h"

namespace crsce::decompress {
    /**
     * @name bool operator
     * @brief bool operator
     * @return bool
     */
    Bits::operator bool() const noexcept {
        return data();
    }
}

