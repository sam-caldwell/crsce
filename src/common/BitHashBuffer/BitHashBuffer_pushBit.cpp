/**
 * @file BitHashBuffer_pushBit.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Push a single bit into the current byte (MSB-first) and advance.
 */
#include "common/BitHashBuffer.h"
#include <cstdint>
#include <cstddef>

namespace crsce::common {

/**
 * @name pushBit
 * @brief Push a single bit into the current byte (MSB-first) and advance.
 * @usage buf.pushBit(true);
 * @throws None
 * @param v Bit value; true=1, false=0.
 * @return N/A
 */
void BitHashBuffer::pushBit(bool v) {
  const int shift = 7 - bitPos_;
  if (v) {
    currByte_ = static_cast<std::uint8_t>(currByte_ | (static_cast<std::uint8_t>(1U) << shift));
  }
  ++bitPos_;
  if (bitPos_ >= 8) {
    flushByteToRow();
    finalizeRowIfFull();
  }
}

} // namespace crsce::common
