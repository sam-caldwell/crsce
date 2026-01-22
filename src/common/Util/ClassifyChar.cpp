/**
 * @file ClassifyChar.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/Util/ClassifyChar.h"
#include "common/Util/CharClass.h" // for direct provider include (include-cleaner)
#include <cctype>

namespace crsce::common::util {
/**
 * @brief Implementation detail.
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
