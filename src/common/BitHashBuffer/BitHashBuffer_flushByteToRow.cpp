/**
 * @file BitHashBuffer_flushByteToRow.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Flush a full currByte_ into the row buffer.
 */
#include "common/BitHashBuffer.h"

namespace crsce::common {

/**
 * @name flushByteToRow
 * @brief Flush a full currByte_ into the row buffer.
 * @usage Internal: called when bitPos_ reaches 8.
 * @throws None
 * @return N/A
 */
void BitHashBuffer::flushByteToRow() {
  if (bitPos_ == 0) {
    return; // No complete byte to flush
  }
  if (bitPos_ < 8) {
    return;
  }
  if (rowIndex_ < kRowSize) {
    rowBuffer_.at(rowIndex_++) = currByte_;
  }
  currByte_ = 0;
  bitPos_ = 0;
}

} // namespace crsce::common
