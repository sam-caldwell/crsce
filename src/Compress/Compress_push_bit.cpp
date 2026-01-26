/**
 * @file Compress_push_bit.cpp
 * @brief Implementation of Compress::push_bit (row-major streaming).
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "compress/Compress/Compress.h"
#include <cstddef>

namespace crsce::compress {
    /**
     * @name Compress::push_bit
     * @brief Push a bit into the block accumulator and update cross-sums/LH.
     * @param bit Data bit value.
     * @return void
     */
    void Compress::push_bit(bool bit) {
        // Update cross-sums on bit=1
        if (bit) {
            lsm_.increment(r_);
            vsm_.increment(c_);
            const std::size_t d = ((r_ + c_) < kS) ? (r_ + c_) : (r_ + c_ - kS);
            dsm_.increment(d);
            const std::size_t x = (r_ >= c_) ? (r_ - c_) : (r_ + kS - c_);
            xsm_.increment(x);
        }

        // Feed LH row hasher (data bit)
        lh_.pushBit(bit);
        ++row_bit_count_;
        ++total_bits_;

        // If a data row reaches 511 bits, append 1 pad bit (zero) and reset row
        // The pad bit finalizes the 512-bit row to 64 bytes and enqueues a digest.
        if (row_bit_count_ >= kBitsPerRow) {
            lh_.pushBit(false);
            row_bit_count_ = 0;
            // Advance to next row at end-of-row; column resets to 0
            ++r_;
            c_ = 0;
        } else {
            // Advance to next column within the same row
            advance_coords_after_bit();
        }
    }
} // namespace crsce::compress
