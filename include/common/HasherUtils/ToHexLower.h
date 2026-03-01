/**
 * @file ToHexLower.h
 * @author Sam Caldwell
 * @brief Convert a 32-byte digest to 64-char lowercase hex.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/HasherUtils/to_hex_lower.h"

namespace crsce::common::hasher {
    /**
     * @name ToHexLowerTag
     * @brief Tag type to satisfy one-definition-per-header for hasher utilities.
     */
    struct ToHexLowerTag {
    };
} // namespace crsce::common::hasher
