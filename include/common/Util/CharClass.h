/**
 * @file CharClass.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstdint>

namespace crsce::common::util {
    /**
     * @name CharClass
     * @brief Classification categories for ASCII characters.
     * @usage CharClass c = CharClass::Digit;
     * @throws None
     * @return N/A
     */
    enum class CharClass : std::uint8_t {
        Digit, ///< '0'..'9'
        Alpha, ///< 'A'..'Z' or 'a'..'z'
        Space, ///< whitespace per std::isspace
        Other ///< any other character
    };
} // namespace crsce::common::util
