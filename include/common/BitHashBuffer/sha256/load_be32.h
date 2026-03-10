/**
 * @file load_be32.h
 * @author Sam Caldwell
 * @brief Load a 32-bit word from big-endian order.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/BitHashBuffer/Sha256Types.h"   // u8
#include "common/BitHashBuffer/Sha256Types32.h" // u32

namespace crsce::common::detail::sha256 {
    /**
     * @name load_be32
     * @brief Load a 32-bit word from big-endian order.
     * @param src Source pointer (>= 4 bytes).
     * @return Parsed 32-bit value.
     */
    u32 load_be32(const u8* src);
}
