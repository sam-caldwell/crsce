/**
 * @file classify_char.h
 * @brief Provider header for classify_char utility function.
 * © Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include "common/Util/CharClass.h"

namespace crsce::common::util {
    /**
     * @name classify_char
     * @brief Classify an input character as Digit, Alpha, Space, or Other.
     * @param ch Character to classify.
     * @return CharClass corresponding to the input character.
     */
    CharClass classify_char(char ch);
} // namespace crsce::common::util
