/**
 * @file ClassifyChar.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include "common/Util/CharClass.h"

namespace crsce::common::util {
    /**
     * @name classify_char
     * @brief Classify an input character as Digit, Alpha, Space, or Other.
     * @usage auto c = classify_char('Z'); // CharClass::Alpha
     * @throws None
     * @param ch Character to classify.
     * @return CharClass corresponding to the input character.
     */
    CharClass classify_char(char ch);
} // namespace crsce::common::util
