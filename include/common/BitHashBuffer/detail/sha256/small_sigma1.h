/**
 * @file small_sigma1.h
 * @brief SHA-256 σ1 function.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/BitHashBuffer/detail/Sha256Types32.h" // u32

namespace crsce::common::detail::sha256 {
    /**
     * @name small_sigma1
     * @brief SHA-256 σ1 function.
     * @param x Input word.
     * @return σ1(x) = ROTR17(x) ^ ROTR19(x) ^ (x >> 10).
     */
    u32 small_sigma1(u32 x);
}
