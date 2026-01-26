/**
 * @file store_be32.h
 * @brief Store a 32-bit word in big-endian order.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/BitHashBuffer/detail/Sha256Types.h"   // u8
#include "common/BitHashBuffer/detail/Sha256Types32.h" // u32

namespace crsce::common::detail::sha256 {
    /**
     * @name store_be32
     * @brief Store a 32-bit word in big-endian order.
     * @param dst Destination pointer (>= 4 bytes).
     * @param x Value to store.
     * @return N/A
     */
    void store_be32(u8 *dst, u32 x);
}
