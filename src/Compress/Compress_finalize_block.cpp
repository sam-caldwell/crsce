/**
 * @file Compress_finalize_block.cpp
 * @brief Implementation of Compress::finalize_block (flush partial row).
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "compress/Compress/Compress.h"

namespace crsce::compress {
    /**
     * @name Compress::finalize_block
     * @brief Finalize the current row if partially filled by padding + 1 LH pad bit.
     * @return void
     */
    void Compress::finalize_block() {
        // Only pad/finalize if we have started a row (i.e., any bits were pushed)
        if (total_bits_ > 0) {
            pad_and_finalize_row_if_needed();
        }
    }
} // namespace crsce::compress
