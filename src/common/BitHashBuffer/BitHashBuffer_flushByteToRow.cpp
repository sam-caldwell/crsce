/**
 * @file BitHashBuffer_flushByteToRow.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Flush a full currByte_ into the row buffer.
 */
#include "../../../include/common/BitHashBuffer/BitHashBuffer.h"

namespace crsce::common {
    /**
     * @name flushByteToRow
     * @brief Flush a full currByte_ into the row buffer.
     * @usage Internal: called when bitPos_ reaches 8.
     * @throws None
     * @return N/A
     */
    void BitHashBuffer::flushByteToRow() {
        if (bitPos_ < 8) {
            return;
        }
        rowBuffer_.at(rowIndex_++) = currByte_;
        currByte_ = 0;
        bitPos_ = 0;
    }
} // namespace crsce::common
