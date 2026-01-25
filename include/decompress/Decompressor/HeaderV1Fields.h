/**
 * @file HeaderV1Fields.h
 * @brief Parsed CRSCE v1 header fields for decompression.
 * © Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>

namespace crsce::decompress {
    /**
     * @name HeaderV1Fields
     * @brief Parsed header fields extracted from a 28-byte CRSCE v1 header.
     */
    struct HeaderV1Fields {
        /** @name original_size_bytes @brief Original uncompressed payload size in bytes. */
        std::uint64_t original_size_bytes{0};
        /** @name block_count @brief Number of blocks in the container. */
        std::uint64_t block_count{0};
    };

} // namespace crsce::decompress
