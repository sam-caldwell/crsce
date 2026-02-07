/**
 * @file Decode9.h
 * @brief Utilities to decode MSB-first 9-bit packed streams.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include "decompress/Utils/detail/decode9.tcc"

namespace crsce::decompress {
    /**
     * @name Decode9Tag
     * @brief Tag type for decode9 utilities (aggregator header).
     */
    struct Decode9Tag {
    };
} // namespace crsce::decompress
