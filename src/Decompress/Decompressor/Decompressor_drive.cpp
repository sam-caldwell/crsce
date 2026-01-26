/**
 * @file Decompressor_drive.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Decompressor/Decompressor.h"
#include "decompress/Decompressor/HeaderV1Fields.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace crsce::decompress {
    /**
     * @brief Implementation detail.
     */
    bool Decompressor::for_each_block(HeaderV1Fields &hdr,
                                      const std::function<void(std::span<const std::uint8_t> lh,
                                                                std::span<const std::uint8_t> sums)> &fn) {
        if (!read_header(hdr)) {
            return false;
        }
        for (std::uint64_t i = 0; i < hdr.block_count; ++i) {
            auto payload = read_block();
            if (!payload.has_value()) {
                return false;
            }
            std::span<const std::uint8_t> lh;
            std::span<const std::uint8_t> sums;
            // read_block() returns exactly kBlockBytes on success
            (void)split_payload(*payload, lh, sums);
            fn(lh, sums);
        }
        return true;
    }
} // namespace crsce::decompress
