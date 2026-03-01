/**
 * @file ExtractFirstHex64.h
 * @author Sam Caldwell
 * @brief Extract first 64 hex chars from text; lowercase output.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/HasherUtils/extract_first_hex64.h"

namespace crsce::common::hasher {
    /**
     * @name ExtractFirstHex64Tag
     * @brief Tag type to satisfy one-definition-per-header for hasher utilities.
     */
    struct ExtractFirstHex64Tag {
    };
} // namespace crsce::common::hasher
