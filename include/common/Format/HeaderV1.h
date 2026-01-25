/**
 * @file HeaderV1.h
 * @brief CRSCE v1 container header (28 bytes LE) packing helper.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace crsce::common::format {
    /**
     * @name HeaderV1
     * @brief Helper struct to pack CRSCE v1 headers (28 bytes LE).
     */
    struct HeaderV1 {
        static constexpr std::size_t kSize = 28;
        static constexpr std::uint16_t kVersion = 1;

        static std::array<std::uint8_t, kSize> pack(std::uint64_t original_size_bytes,
                                                    std::uint64_t block_count);
    };
} // namespace crsce::common::format
