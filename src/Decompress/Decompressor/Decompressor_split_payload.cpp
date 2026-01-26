/**
 * @file Decompressor_split_payload.cpp
 * @brief Implementation of Decompressor::split_payload.
 * © Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Decompressor/Decompressor.h"

#include <cstdint>
#include <span>
#include <vector>

namespace crsce::decompress {
    /**
     * @name Decompressor::split_payload
     * @brief Split a block payload into LH and cross-sum spans if size matches kBlockBytes.
     * @param block Full block payload buffer.
     * @param out_lh Output span over LH bytes.
     * @param out_sums Output span over cross-sum bytes.
     * @return bool True if the block size is valid; false otherwise.
     */
    bool Decompressor::split_payload(const std::vector<std::uint8_t> &block,
                                     std::span<const std::uint8_t> &out_lh,
                                     std::span<const std::uint8_t> &out_sums) {
        if (block.size() != kBlockBytes) {
            return false;
        }
        const std::span<const std::uint8_t> sp{block};
        out_lh = sp.first(kLhBytes);
        out_sums = sp.subspan(kLhBytes, kSumsBytes);
        return true;
    }
} // namespace crsce::decompress
