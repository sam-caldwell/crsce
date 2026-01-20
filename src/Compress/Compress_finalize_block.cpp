/**
 * @file Compress_finalize_block.cpp
 * @brief Implementation of Compress::finalize_block (flush partial row).
 */
#include "compress/Compress/Compress.h"

namespace crsce::compress {
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

    void Compress::finalize_block() {
        // Only pad/finalize if we have started a row (i.e., any bits were pushed)
        if (total_bits_ > 0) {
            pad_and_finalize_row_if_needed();
        }
    }
} // namespace crsce::compress
