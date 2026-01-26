/**
 * @file rotr.h
 * @brief SHA-256 rotate-right utility for 32-bit values.
 * @copyright Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/BitHashBuffer/detail/Sha256Types32.h" // u32

namespace crsce::common::detail::sha256 {
    /**
     * @name rotr
     * @brief Rotate-right utility for 32-bit values.
     * @param x Input word.
     * @param n Rotation amount [0..31].
     * @return x rotated right by n bits.
     */
    u32 rotr(u32 x, u32 n);
}
