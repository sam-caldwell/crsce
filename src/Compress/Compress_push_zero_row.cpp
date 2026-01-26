/**
 * @file Compress_push_zero_row.cpp
 * @brief Implementation of Compress::push_zero_row.
 * © Sam Caldwell. See LICENSE.txt for details.
 */
#include "compress/Compress/Compress.h"
#include <cstddef>

namespace crsce::compress {
    /**
     * @name Compress::push_zero_row
     * @brief Push a full row of zeros and finalize its LH.
     * @return void
     */
    void Compress::push_zero_row() {
        for (std::size_t i = 0; i < kBitsPerRow; ++i) {
            push_bit(false);
        }
    }
} // namespace crsce::compress
