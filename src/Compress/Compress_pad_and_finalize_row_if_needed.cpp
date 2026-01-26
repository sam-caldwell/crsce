/**
 * @file Compress_pad_and_finalize_row_if_needed.cpp
 * @brief Implementation of Compress::pad_and_finalize_row_if_needed.
 * © Sam Caldwell. See LICENSE.txt for details.
 */
#include "compress/Compress/Compress.h"

namespace crsce::compress {
    /**
     * @name Compress::pad_and_finalize_row_if_needed
     * @brief If a partial row exists, pad zeros and finalize its LH block.
     * @return void
     */
    void Compress::pad_and_finalize_row_if_needed() {
        if (row_bit_count_ == 0) {
            return; // no partial row to flush
        }
        // Pad remaining data bits in the row to reach 511, then 1 zero pad bit
        while (row_bit_count_ < kBitsPerRow) {
            lh_.pushBit(false);
            ++row_bit_count_;
        }
        // 1 pad bit to complete 512 bits
        lh_.pushBit(false);
        row_bit_count_ = 0;
        ++r_;
        c_ = 0;
    }
} // namespace crsce::compress
