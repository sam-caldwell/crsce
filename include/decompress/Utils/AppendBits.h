/**
 * @file AppendBits.h
 * @brief Helpers to append CSM bits into a byte stream (MSB-first).
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <vector>
#include "decompress/Utils/detail/append_bits_from_csm.h"

namespace crsce::decompress {
    /**
     * @name AppendBitsTag
     * @brief Tag type for append_bits utilities (aggregator header).
     */
    struct AppendBitsTag {
    };

    // Implementation declared in detail/append_bits_from_csm.h
} // namespace crsce::decompress
