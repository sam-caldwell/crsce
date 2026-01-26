/**
 * @file Compress_serialize_cross_sums.cpp
 * @brief Serialize LSM/VSM/DSM/XSM as a contiguous byte vector.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "compress/Compress/Compress.h"
#include <cstdint>
#include <vector>

namespace crsce::compress {
    /**
     * @name Compress::serialize_cross_sums
     * @brief Serialize cross-sums in order LSM,VSM,DSM,XSM into a byte vector.
     * @return std::vector<std::uint8_t> Concatenated cross-sum bytes (4*575 bytes).
     */
    std::vector<std::uint8_t> Compress::serialize_cross_sums() const {
        std::vector<std::uint8_t> out;
        out.reserve(4 * 575); // 4 vectors * 575 bytes each
        lsm_.serialize_append(out);
        vsm_.serialize_append(out);
        dsm_.serialize_append(out);
        xsm_.serialize_append(out);
        return out;
    }
} // namespace crsce::compress
