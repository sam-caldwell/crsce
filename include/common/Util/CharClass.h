/**
 * @file CharClass.h
 * @brief define a character class enum
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstdint>

namespace crsce::common::util {
    /**
     * @name CharClass
     * @brief Classification categories for ASCII characters.
     * @usage CharClass c = CharClass::Digit;
     */
    enum class CharClass : std::uint8_t {
        Digit, ///< '0'..'9'
        Alpha, ///< 'A'..'Z' or 'a'..'z'
        Space, ///< whitespace per std::isspace
        Other ///< any other character
    };
} // namespace crsce::common::util
