/**
 * @file FileBitSerializer_has_next.cpp
 * © 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Implementation of FileBitSerializer::has_next.
 */
#include "common/FileBitSerializer/FileBitSerializer.h"

namespace crsce::common {

/**
 * @name has_next
 * @brief Check if at least one more bit can be produced (may refill).
 * @usage While (s.has_next()) { auto b = s.pop(); }
 * @throws None
 * @return true if another bit is available; false at EOF.
 */
bool FileBitSerializer::has_next() {
  if (byte_pos_ < buf_len_) {
    return true;
  }
  if (eof_) {
    return false;
  }
  // If not at EOF, attempt a refill; a successful fill implies buf_len_ > 0.
  return fill();
}

} // namespace crsce::common
