/**
 * @file ClassifyChar.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/Util/detail/classify_char.h"
#include "common/Util/CharClass.h" // direct provider include (include-cleaner)
#include <cctype>

namespace crsce::common::util {
/**
 * @name classify_char
 * @brief Classify a character using the C locale into Digit, Alpha, Space, or Other.
 * @param ch Character to classify.
 * @return CharClass enum indicating the category.
 * @details Wraps <cctype> predicates and normalizes the return type for parsing utilities.
 */
CharClass classify_char(const char ch) {
  const auto uch = static_cast<unsigned char>(ch);
  if (std::isdigit(uch) != 0) {
    return CharClass::Digit;
  }
  if (std::isalpha(uch) != 0) {
    return CharClass::Alpha;
  }
  if (std::isspace(uch) != 0) {
    return CharClass::Space;
  }
  return CharClass::Other;
}
} // namespace crsce::common::util
