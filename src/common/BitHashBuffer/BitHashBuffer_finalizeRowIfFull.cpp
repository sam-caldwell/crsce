/**
 * @file BitHashBuffer_finalizeRowIfFull.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Compute per-row hash (no chaining) when a 64-byte row completes.
 */
#include "../../../include/common/BitHashBuffer/BitHashBuffer.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"

#include <array>

namespace crsce::common {
    /**
     * @name finalizeRowIfFull
     * @brief If 64 bytes accumulated, compute chained hash and reset the row.
     * @usage Internal: invoked after flushing bytes to check for row completion.
     * @throws None
     * @return N/A
     */
    void BitHashBuffer::finalizeRowIfFull() {
        if (rowIndex_ < kRowSize) {
            return;
        }
        // New semantics: per-row digest only (sha256(rowBuffer_)); seeding/chain disabled.
        const auto digest = detail::sha256::sha256_digest(rowBuffer_.data(), rowBuffer_.size());
        hashVector_.push_back(digest);

        rowBuffer_.fill(0);
        rowIndex_ = 0;
    }
} // namespace crsce::common
