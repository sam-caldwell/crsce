/**
 * @file Compress_advance_coords_after_bit.cpp
 * @brief Advance coordinates after a bit within a row.
 */
#include "compress/Compress/Compress.h"

namespace crsce::compress {
    void Compress::advance_coords_after_bit() {
        // Advance column within the same row; wrap at kS (but row resets only at end-of-row)
        ++c_;
    }
} // namespace crsce::compress
