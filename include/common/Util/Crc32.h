/**
 * @file Crc32.h
 * @brief CRC-32 (IEEE 802.3) utility.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include "common/Util/detail/crc32_ieee.h"

namespace crsce::common::util {
    /**
     * @name Crc32Tag
     * @brief Tag type to satisfy one-definition-per-header for CRC utilities.
     */
    struct Crc32Tag {
    };
} // namespace crsce::common::util
