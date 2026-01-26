/**
 * @file Compress_advance_coords_after_bit.cpp
 * @brief Advance coordinates after a bit within a row.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "compress/Compress/Compress.h"

namespace crsce::compress {
    /**
     * @name Compress::advance_coords_after_bit
     * @brief Advance the column coordinate after pushing a bit.
     * @return void
     */
    void Compress::advance_coords_after_bit() {
        // Advance column within the same row; wrap at kS (but row resets only at end-of-row)
        ++c_;
    }
} // namespace crsce::compress
